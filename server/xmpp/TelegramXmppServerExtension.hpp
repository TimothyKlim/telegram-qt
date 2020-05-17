#ifndef TELEGRAM_XMPP_SERVER_EXTENSION_HPP
#define TELEGRAM_XMPP_SERVER_EXTENSION_HPP

#include <QXmppServerExtension.h>

class QXmppMessage;
class QXmppDiscoveryIq;
class QXmppMessage;
class QXmppPingIq;
class QXmppServer;
class QXmppIq;

namespace Telegram {

namespace Server {

class XmppFederalizationApi;

class TelegramExtension : public QXmppServerExtension
{
public:
    TelegramExtension(XmppFederalizationApi *parent);

    QString extensionName() const override;
    int extensionPriority() const override;
//    QStringList discoveryFeatures() const override;
//    QStringList discoveryItems() const override;

    QString discoverUser(const QString &jid);
    QString pingUser(const QString &jid);

    bool handleStanza(const QDomElement &element) override;
    bool handleMessage(const QXmppMessage &stanza);
    bool handleDiscoveryResult(const QXmppDiscoveryIq &stanza);
    bool handlePingResult(const QXmppIq &stanza);
    bool handleRequestVCard(const QDomElement &element);

//    QSet<QString> presenceSubscribers(const QString &jid) override;
//    QSet<QString> presenceSubscriptions(const QString &jid) override;

    void processIncomingMessage(const QXmppMessage &message);

    XmppFederalizationApi *xmpp() const { return m_xmpp; }

protected:
    XmppFederalizationApi *m_xmpp = nullptr;
};

} // Server

} // Telegram

#endif // TELEGRAM_XMPP_SERVER_EXTENSION_HPP
