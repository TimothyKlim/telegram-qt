#ifndef FEDERALIZATION_SERVER_API
#define FEDERALIZATION_SERVER_API

#include "ServerApi.hpp"
#include "RemoteServerConnection.hpp"

#include <QSet>

namespace Telegram {

namespace Server {

class FederalizationApi : public AbstractServerApi
{
public:
    FederalizationApi() = default;
    ~FederalizationApi() = default;

    virtual bool start() = 0;

    quint32 dcId() const final { return m_dcId; }
    void setDcId(quint32 id) { m_dcId = id; }

    MessageService *messageService() const override { return m_messageService; }
    void setMessageService(MessageService *messageService) { m_messageService = messageService; }
    IMediaService *mediaService() const override { return nullptr; }

    void addServerConnection(AbstractServerConnection *remoteServer);

    AbstractUser *getServerUser(quint32 userId) const;
    AbstractUser *getServerUser(const QString &userName) const;
    AbstractServerConnection *getServerForPeer(const Peer &peer) const;

    GroupChat *getGroupChat(quint32 chatId) const override;

protected:
    void processCreateChat(const UpdateNotification &notification);

private:
    quint32 m_dcId = 0;
    QSet<AbstractServerConnection*> m_remoteServers;
    MessageService *m_messageService = nullptr;
    QHash<quint32, LocalGroupChat *> m_groups; // groupId to GroupChat
};

class FederalizationGateway : public AbstractServerConnection
{
    Q_OBJECT
public:
    explicit FederalizationGateway(QObject *parent = nullptr);

    quint32 dcId() const override;

    void setApi(FederalizationApi *remoteServer);

    AbstractUser *getUser(const quint32 userId) const override;
    AbstractUser *getUser(const QString &identifier) const override;
    AbstractServerApi *api();

    QByteArray getForeingUserAuthorization(quint32 userId);

protected:
    FederalizationApi *m_server = nullptr;
};

} // Server namespace

} // Telegram

#endif // FEDERALIZATION_SERVER_API
