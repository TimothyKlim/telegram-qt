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

    XmppUser *ensureUser(const QString &jid);

    XmppUser *getUser(const QString &jid) const;
    XmppUser *getUser(quint32 userId) const;

    AbstractUser *getTelegramUser(const QString &jid);
    AbstractUser *getTelegramUser(quint32 userId);
    QString getBareJid(quint32 userId);

    QXmppServer *xmppServer() const { return m_xmppServer; }
    void sendMessageFromTelegram(const QString &from, const QString &toJid, const QString &message);
    void sendMessageFromXmpp(XmppUser *fromUser, MessageRecipient *recipient, const QString &message);

    AbstractUser *getAbstractUser(quint32 userId) const override;
    AbstractUser *getAbstractUser(const QString &identifier) const override;

    void insertUser(XmppUser *user);

    PhoneStatus getPhoneStatus(const QString &identifier) const override { return PhoneStatus(); }
    bool identifierIsValid(const QString &identifier) const override { return false; }
    QString normalizeIdentifier(const QString &identifier) const override { return QString(); }
    IMediaService *mediaService() const override { return nullptr; }
    MessageRecipient *getRecipient(const Peer &peer) const override { return nullptr; }
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
