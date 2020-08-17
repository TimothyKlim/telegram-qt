#include <QtPlugin>

#define FederalizationInterface_iid "TelegramQt.Server.FederalizationInterface/1.0"

namespace Telegram {

namespace Server {

class FederalizationApi;

class FederalizationInterface
{
public:
    virtual FederalizationApi *create() = 0;
};

} // Server

} // Telegram

Q_DECLARE_INTERFACE(Telegram::Server::FederalizationInterface, FederalizationInterface_iid)

class FederalizationPlugin : public QObject
        , public Telegram::Server::FederalizationInterface
{
    Q_OBJECT
    Q_INTERFACES(Telegram::Server::FederalizationInterface)
public:
    explicit FederalizationPlugin(QObject *parent = nullptr);
    ~FederalizationPlugin() override;

    Telegram::Server::FederalizationApi *create() override = 0;
};
