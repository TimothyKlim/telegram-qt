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

#include "GroupChat.hpp"
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

AbstractUser *XmppFederalizationApi::getUser(const QString &jid) const
{
    XmppUser *user = getXmppUser(jid);
    if (user) {
        return user;
    }

    return getTelegramUser(jid);
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

XmppUser *XmppFederalizationApi::getXmppUser(const QString &jid) const
{
    const QString bareJid = QXmppUtils::jidToBareJid(jid);
    quint32 userId = m_jidToUserId.value(bareJid);
    return m_users.value(userId);
}

XmppUser *XmppFederalizationApi::getXmppUser(quint32 userId) const
{
    return m_users.value(userId);
}

GroupChat *XmppFederalizationApi::getTelegramChat(const QString &jid) const
{
    const QString bareJid = QXmppUtils::jidToBareJid(jid);
    quint32 chatId = m_jidToChatId.value(bareJid);
    return getTelegramChat(chatId);
}

GroupChat *XmppFederalizationApi::getTelegramChat(quint32 chatId) const
{
    return getGroupChat(chatId);
}

AbstractUser *XmppFederalizationApi::getTelegramUser(const QString &jid) const
{
    const QString userName = QXmppUtils::jidToUser(jid);
    AbstractUser *user = getServerUser(userName);
    if (!user) {
        qWarning() << "User" << userName << "Not found on the Telegram side";
    }
    return user;
}

AbstractUser *XmppFederalizationApi::getTelegramUser(quint32 userId) const
{
    return getServerUser(userId);
}

QString XmppFederalizationApi::getUserBareJid(quint32 userId) const
{
    const XmppUser *localUser = getXmppUser(userId);
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

QString XmppFederalizationApi::getBareJid(const Peer &peer) const
{
    if (peer.type() == Peer::User) {
        return getUserBareJid(peer.id());
    } else if (peer.type() == Peer::Chat) {
        if (!m_chatIdToJid.contains(peer.id())) {
            if (getTelegramChat(peer.id())) {
                QString chatPart = peer.toString();
                QString domainPart = domain();
                m_chatIdToJid.insert(peer.id(), QStringLiteral("%1@%2").arg(chatPart, domainPart));
            }
        }
        return m_chatIdToJid.value(peer.id());
    }

    return QString();
}

Peer XmppFederalizationApi::getPeerFromJid(const QString &jid) const
{
    AbstractUser *user = getUser(jid);
    if (user) {
        return user->toPeer();
    }

    return Peer();
}

void XmppFederalizationApi::sendMessageFromTelegram(const QString &from, const Peer &targetPeer, const QString &routeToJid, const MessageData *messageData)
{
    if (from.isEmpty()) {
        return;
    }

    const QString toJid = getBareJid(targetPeer);
    if (toJid != routeToJid) {
        qCritical() << "Unable to delivery the message to" << toJid << "for jid" << routeToJid;
        return;
    }

    xmppServer()->sendPacket(QXmppMessage(from, toJid, messageData->content().text()));
}

void XmppFederalizationApi::sendMessageFromXmpp(XmppUser *fromUser, const Peer &targetPeer, const QString &message)
{
    const MessageData *messageData = messageService()->addMessage(fromUser->id(),
                                                                  targetPeer,
                                                                  message);
    const quint32 requestDate = Telegram::Utils::getCurrentTime();
    const QVector<PostBox *> boxes = getPostBoxes(targetPeer, fromUser);

    for (const PostBox *box : boxes) {
        if (box->peer() == fromUser->toPeer()) {
            // Ignore the XMPP post box
            continue;
        }

        UpdateNotification notification;
        notification.type = UpdateNotification::Type::NewMessage;
        notification.date = requestDate;
        notification.messageDataId = messageData->globalId();
        notification.userId = box->peer().id();
        notification.dialogPeer = fromUser->toPeer();

        AbstractServerApi *remoteApi = getServerApiForPeer(box->peer());
        if (!remoteApi) {
            continue;
        }
        remoteApi->queueServerUpdates({notification});
    }
}

void XmppFederalizationApi::inviteToMuc(const Peer &mucPeer, const QString &fromJid, const QString &toJid)
{
    // XEP-0249: Direct MUC Invitations
    const QString mucJid = getBareJid(mucPeer);
    QXmppMessage message(fromJid, toJid);
    message.setMucInvitationJid(mucJid);

    xmppServer()->sendPacket(message);


    // https://github.com/psi-im/psi/blob/85eef3b47d3cc2a9b7b942c1548da71c4c77a0a1/src/psiaccount.cpp#L2782

    // what came:
    // <message type="chat"
    //     from="123456789@192.168.1.183"
    //     to="test1@127.0.0.3">
    // <x xmlns="jabber:x:conference"
    //     jid="chat1@192.168.1.183"/>
    // </message>


    // RECEIVED <message to="test1@127.0.0.3" from="123456789@192.168.1.183" type="chat"><x xmlns="jabber:x:conference" jid="chat1@192.168.1.183"/></message>
    // SENT <message to="test1@127.0.0.3" type="chat" from="123456789@192.168.1.183"><x xmlns="jabber:x:conference" jid="chat1@192.168.1.183"/></message>

    // RECEIVED <message from="123456789@192.168.1.183" to="test1@127.0.0.3">
    // <x xmlns="jabber:x:conference" jid="chat1@192.168.1.183"/>
    // </message>
    // WARNING Received a stanza from unexpected JID 123456789@192.168.1.183
}

AbstractUser *XmppFederalizationApi::getAbstractUser(quint32 userId) const
{
    return getXmppUser(userId);
}

AbstractUser *XmppFederalizationApi::getAbstractUser(const QString &identifier) const
{
    return getXmppUser(identifier);
}

void XmppFederalizationApi::insertUser(XmppUser *user)
{
    qInfo(lcServerXmpp) << __func__ << user->userId() << user->jid();
    m_users.insert(user->userId(), user);
    m_jidToUserId.insert(user->jid(), user->userId());
}

QVector<PostBox *> XmppFederalizationApi::getPostBoxes(const Peer &targetPeer, AbstractUser *applicant) const
{
    // Boxes:
    // message to contact
    //    Users (self and recipient (if not self))
    //
    // message to group chat
    //    Users (each member)
    //
    // message to megagroup or broadcast
    //    Channel (the channel)

    QVector<PostBox *> boxes;
    if (targetPeer.type() == Peer::User) {
        AbstractUser *toUser = getTelegramUser(targetPeer.id());
        boxes.append(toUser->getPostBox());
        if (applicant && applicant->id() != targetPeer.id()) {
            boxes.append(applicant->getPostBox());
        }
    }
    if (targetPeer.type() == Peer::Chat) {
        const GroupChat *groupChat = getTelegramChat(targetPeer.id());
        for (const quint32 userId : groupChat->memberIds()) {
            AbstractUser *telegramParticipant = getTelegramUser(userId);
            if (telegramParticipant) {
                boxes.append(telegramParticipant->getPostBox());
            }
        }
    }

    return boxes;
}

void XmppFederalizationApi::queueServerUpdates(const QVector<UpdateNotification> &notifications)
{
    for (const UpdateNotification &notification : notifications) {
        XmppUser *user = getXmppUser(notification.userId);
        if (!user) {
            qWarning(lcServerXmpp) << __func__ << "Invalid user" << notification.userId;
            continue;
        }

        switch (notification.type) {
        case UpdateNotification::Type::CreateChat: {
            processCreateChat(notification);
            const MessageData *messageData = messageService()->getMessage(notification.messageDataId);
            QString from = getUserBareJid(messageData->fromId());
            for (const quint32 invitedId : messageData->action().users) {
                XmppUser *invitedXmppUser = getXmppUser(invitedId);
                if (!invitedXmppUser) {
                    continue;
                }
                inviteToMuc(notification.dialogPeer, from, invitedXmppUser->jid());
            }
        }
            break;
        case UpdateNotification::Type::NewMessage: {
            const MessageData *messageData = messageService()->getMessage(notification.messageDataId);
            const QString to = getUserBareJid(notification.userId);
            const QString from = getUserBareJid(messageData->fromId());
            const Peer peer = messageData->getDialogPeer(messageData->fromId());
            sendMessageFromTelegram(from, peer, to, messageData);
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
