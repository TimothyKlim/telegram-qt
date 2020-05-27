#ifndef XMPP_FEDERATION_HPP
#define XMPP_FEDERATION_HPP

#include "FederalizationApi.hpp"

#include <QHostAddress>

class QXmppServer;

namespace Telegram {

namespace Server {

class AbstractUser;
class TelegramExtension;
class XmppUser;

class XmppFederalizationApi : public FederalizationApi
{
    Q_OBJECT
public:
    XmppFederalizationApi(QObject *parent = nullptr);
    ~XmppFederalizationApi();

    bool start() override;

    QStringList supportedSchemes() const override;
    PendingVariant *searchContacts(const QString &query, quint32 limit);

    QString domain() const;
    void setDomain(const QString &domain);
    void setListenAddress(const QHostAddress &address);

    AbstractUser *getUser(const QString &jid) const;

    XmppUser *ensureUser(const QString &jid);

    XmppUser *getXmppUser(const QString &jid) const;
    XmppUser *getXmppUser(quint32 userId) const;

    AbstractUser *getTelegramUser(const QString &jid) const;
    AbstractUser *getTelegramUser(quint32 userId) const;
    QString getUserBareJid(quint32 userId) const;
    QString getBareJid(const Peer &peer) const;
    Peer getPeerFromJid(const QString &jid) const;

    QXmppServer *xmppServer() const { return m_xmppServer; }
    void sendMessageFromTelegram(const QString &from, const Peer &targetPeer, const QString &routeToJid, const MessageData *messageData);
    void sendMessageFromXmpp(XmppUser *fromUser, const Peer &targetPeer, const QString &message);

    void inviteToMuc(const Peer &mucPeer, const QString &fromJid, const QString &toJid);

    AbstractUser *getAbstractUser(quint32 userId) const override;
    AbstractUser *getAbstractUser(const QString &identifier) const override;

    void insertUser(XmppUser *user);
    QVector<PostBox *> getPostBoxes(const Peer &targetPeer, AbstractUser *applicant = nullptr) const;

    PhoneStatus getPhoneStatus(const QString &identifier) const override { return PhoneStatus(); }
    bool identifierIsValid(const QString &identifier) const override { return false; }
    QString normalizeIdentifier(const QString &identifier) const override { return QString(); }
    void queueServerUpdates(const QVector<UpdateNotification> &notifications) override;

    void addNetworkRequest(const QString &stanzaId, PendingVariant *operation);
    PendingVariant *takeRequest(const QString &stanzaId);

protected:
    QXmppServer *m_xmppServer = nullptr;
    TelegramExtension *m_telegramExtension = nullptr;
    QHostAddress m_listenAddress;

    // Data
    QHash<quint32, XmppUser*> m_users; // userId to User

    // Maps for faster lookup
    QHash<QString, quint32> m_jidToUserId;

    QHash<QString, PendingVariant *> m_requests;
};

} // Server

} // Telegram

#endif // XMPP_FEDERATION_HPP
