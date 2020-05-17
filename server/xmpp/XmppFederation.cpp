#include "XmppFederation.hpp"

#include <QVariantMap>
#include <QHostAddress>
#include <QSslKey>

#include <QXmppDiscoveryIq.h>
#include <QXmppPingIq.h>
#include <QXmppMessage.h>
#include <QXmppServer.h>
#include <QXmppUtils.h>

#include "TelegramXmppServerExtension.hpp"
#include "XmppUser.hpp"

#include "ApiUtils.hpp"

#include "MessageService.hpp"
#include "PendingVariant.hpp"
#include "ServerApi.hpp"
#include <QDomElement>

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(lcServerXmpp, "telegram.server.cross.xmpp", QtDebugMsg)

static const QString c_xmpp = QLatin1String("xmpp");

namespace Telegram {

namespace Server {

XmppFederalizationApi::XmppFederalizationApi(QObject *parent)
    : FederalizationApi(parent)
    , m_xmppServer(new QXmppServer(this))
{
    //QXmppLogger *logger = new QXmppLogger();
    //logger->setLoggingType(QXmppLogger::StdoutLogging);
    //m_xmppServer->setLogger(logger);

    m_telegramExtension = new TelegramExtension(this);
    m_xmppServer->addExtension(m_telegramExtension);
}

XmppFederalizationApi::~XmppFederalizationApi()
{
    delete m_xmppServer;
}

bool XmppFederalizationApi::start()
{
    bool started = false;
    quint16 port = 5269;
    if (m_listenAddress.isNull()) {
        started = m_xmppServer->listenForServers();
    } else {
        started = m_xmppServer->listenForServers(m_listenAddress, port);
    }

    qCInfo(lcServerXmpp).nospace().noquote() << "Start XMPP Gateway (DC " << dcId() << ") "
                                                      << "(listening " << m_listenAddress.toString()
                                                      << ":" <<port << ")";

    return started;
}

QStringList XmppFederalizationApi::supportedSchemes() const
{
    return { c_xmpp };
}

bool isCompleteJid(const QString &jid)
{
    if (jid.indexOf(QLatin1Char('@')) < 0) {
        return false;
    }

    QString contactDomain = QXmppUtils::jidToDomain(jid);
    if (contactDomain.count(QLatin1Char('.')) < 3) {
        return false;
    }

    // We work only with IP addresses for now.
    if (!contactDomain.at(contactDomain.length() - 1).isDigit()) {
        return false;
    }

    return true;
}

PendingVariant *XmppFederalizationApi::searchContacts(const QString &query, quint32 limit)
{
    Q_UNUSED(limit)

    if (!query.startsWith(QLatin1String("xmpp:"))) {
        QString text = QLatin1String("Invalid search request");
        return PendingOperation::failOperation<PendingVariant>(text, this);
    }

    QString jid = query.section(QLatin1Char(':'), 1);
    if (!isCompleteJid(jid)) {
        QString text = QLatin1String("Incomplete JID");
        return PendingOperation::failOperation<PendingVariant>(text, this);
    }

    // QString requestId = m_telegramExtension->discoverUser(jid);
    QString requestId = m_telegramExtension->pingUser(jid);
    if (requestId.isEmpty()) {
        QString text = QLatin1String("Unknown error (unable to send request)");
        return PendingOperation::failOperation<PendingVariant>(text, this);
    }

    PendingVariant *op = new PendingVariant(this);
    m_requests.insert(requestId, op);

    return op;
}

QString XmppFederalizationApi::domain() const
{
    return m_xmppServer->domain();
}

void XmppFederalizationApi::setDomain(const QString &domain)
{
    m_xmppServer->setDomain(domain);
}

void XmppFederalizationApi::setListenAddress(const QHostAddress &address)
{
    m_listenAddress = address;
}

XmppUser *XmppFederalizationApi::ensureUser(const QString &jid)
{
    const QString bareJid = QXmppUtils::jidToBareJid(jid);
    quint32 userId = m_jidToUserId.value(bareJid);

    if (userId) {
        return m_users.value(userId);
    }

    XmppUser *newUser = new XmppUser();
    newUser->setJid(bareJid);
    newUser->setDcId(dcId());
    insertUser(newUser);
    return newUser;
}

XmppUser *XmppFederalizationApi::getUser(const QString &jid) const
{
    const QString bareJid = QXmppUtils::jidToBareJid(jid);
    quint32 userId = m_jidToUserId.value(bareJid);
    return m_users.value(userId);
}

XmppUser *XmppFederalizationApi::getUser(quint32 userId) const
{
    return m_users.value(userId);
}

AbstractUser *XmppFederalizationApi::getTelegramUser(const QString &jid)
{
    const QString userName = QXmppUtils::jidToUser(jid);
    AbstractUser *user = getServerUser(userName);
    if (!user) {
        qWarning() << "User" << userName << "Not found on the Telegram side";
    }
    return user;
}

AbstractUser *XmppFederalizationApi::getTelegramUser(quint32 userId)
{
    return getServerUser(userId);
}

QString XmppFederalizationApi::getBareJid(quint32 userId)
{
    const XmppUser *localUser = getUser(userId);
    if (localUser) {
        return localUser->jid();
    }
    AbstractUser *telegramUser = getTelegramUser(userId);
    if (telegramUser) {
        QString userPart = telegramUser->phoneNumber();
        QString domainPart = domain();
        return QStringLiteral("%1@%2").arg(userPart, domainPart);
    }

    qWarning(lcServerXmpp) << "User not found" << userId;
    return QString();
}

void XmppFederalizationApi::sendMessageFromTelegram(const QString &from, const QString &toJid, const QString &message)
{
    if (from.isEmpty()) {
        return;
    }
    if (toJid.isEmpty()) {
        return;
    }

    xmppServer()->sendPacket(QXmppMessage(from, toJid, message));
}

void XmppFederalizationApi::sendMessageFromXmpp(XmppUser *fromUser, MessageRecipient *recipient, const QString &message)
{
    const MessageData *messageData = messageService()->addMessage(fromUser->id(),
                                                                  recipient->toPeer(),
                                                                  message);
    const quint32 requestDate = Telegram::Utils::getCurrentTime();
    const QVector<PostBox *> boxes = recipient->postBoxes();

    for (const PostBox *box : boxes) {
        UpdateNotification notification;
        notification.type = UpdateNotification::Type::NewMessage;
        notification.date = requestDate;
        notification.messageDataId = messageData->globalId();
        notification.userId = box->peer().id();
        notification.dialogPeer = fromUser->toPeer();

        AbstractServerConnection *remoteServerConnection = getServerForPeer(box->peer());
        AbstractServerApi *remoteApi = remoteServerConnection->api();
        remoteApi->queueServerUpdates({notification});
    }
}

AbstractUser *XmppFederalizationApi::getAbstractUser(quint32 userId) const
{
    return getUser(userId);
}

AbstractUser *XmppFederalizationApi::getAbstractUser(const QString &identifier) const
{
    return getUser(identifier);
}

void XmppFederalizationApi::insertUser(XmppUser *user)
{
    qInfo(lcServerXmpp) << __func__ << user->userId() << user->jid();
    m_users.insert(user->userId(), user);
    m_jidToUserId.insert(user->jid(), user->userId());
}

void XmppFederalizationApi::queueServerUpdates(const QVector<UpdateNotification> &notifications)
{
    for (const UpdateNotification &notification : notifications) {
        XmppUser *user = getUser(notification.userId);
        if (!user) {
            qWarning(lcServerXmpp) << __func__ << "Invalid user" << notification.userId;
            continue;
        }

        switch (notification.type) {
        case UpdateNotification::Type::NewMessage: {
            const MessageData *messageData = messageService()->getMessage(notification.messageDataId);
            QString to = getBareJid(notification.userId);
            QString from = getBareJid(messageData->fromId());
            sendMessageFromTelegram(from, to, messageData->content().text());
        }
            break;
        default:
            qWarning(lcServerXmpp) << __func__ << "Not implemented:" << notification.type;
            break;
        }
    }
}

void XmppFederalizationApi::addNetworkRequest(const QString &stanzaId, PendingVariant *operation)
{
    m_requests.insert(stanzaId, operation);
}

PendingVariant *XmppFederalizationApi::takeRequest(const QString &stanzaId)
{
    return m_requests.take(stanzaId);
}

} // Server

} // Telegram
