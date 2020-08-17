#include "FederalizationPlugin.hpp"

#include "XmppFederation.hpp"

namespace Telegram {

namespace Server {

class XmppPlugin : public FederalizationPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID FederalizationInterface_iid)
public:
    FederalizationApi *create() final
    {
        return new XmppFederalizationApi();
    }
};

} // Server

} // Telegram

#include "XmppPlugin.moc"
