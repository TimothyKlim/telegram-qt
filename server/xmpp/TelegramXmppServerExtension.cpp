#include "TelegramXmppServerExtension.hpp"

#include <QTextStream>
#include <QDomElement>
#include <QLoggingCategory>

#include <QXmppDiscoveryIq.h>
#include <QXmppIq.h>
#include <QXmppMessage.h>
#include <QXmppPingIq.h>
#include <QXmppPresence.h>
#include <QXmppServer.h>
#include <QXmppVCardIq.h>

#include "PendingVariant.hpp"

#include "XmppFederation.hpp"
#include "XmppUser.hpp"

#include <QXmlStreamWriter>
#include <QBuffer>

template <class T>
static void printXmppPacket(T &packet)
{
    QBuffer buffer;
    buffer.open(QIODevice::ReadWrite);
    QXmlStreamWriter writer(&buffer);
    writer.setAutoFormatting(true);
    packet.toXml(&writer);
    qCritical().noquote() << "writing" << buffer.data();
}

namespace Telegram {

namespace Server {

TelegramExtension::TelegramExtension(XmppFederalizationApi *parent)
    : m_xmpp(parent)
{
}

QString TelegramExtension::extensionName() const
{
    return QStringLiteral("Telegram");
}

int TelegramExtension::extensionPriority() const
{
    return 10;
}

QString TelegramExtension::discoverUser(const QString &jid)
{
    // <iq type='get'
    //     from='shakespeare.lit'
    //     to='juliet@capulet.com'
    //     id='info2'>
    //   <query xmlns='http://jabber.org/protocol/disco#info'/>
    // </iq>

    QXmppDiscoveryIq iq;
    iq.setTo(jid);
    iq.setFrom(server()->domain());

    printXmppPacket(iq);

    server()->sendPacket(iq);
    return iq.id();
}

QString TelegramExtension::pingUser(const QString &jid)
{
    QXmppPingIq iq;
    iq.setTo(jid);
    iq.setFrom(server()->domain());

    printXmppPacket(iq);

    server()->sendPacket(iq);
    return iq.id();
}

bool TelegramExtension::handleStanza(const QDomElement &element)
{
    QString str;
    QTextStream stream(&str);
    element.save(stream, 4);

    qDebug().noquote() << "handleStanza:\n" << str;


    if (element.tagName() == "message") {
        QXmppMessage message;
        message.parse(element);
        bool succeeded = handleMessage(message);
        if (!succeeded) {
            QXmppIq response(QXmppIq::Error);
            response.setId(message.id());
            response.setFrom(server()->domain());
            response.setTo(message.from());
            QXmppStanza::Error error(QXmppStanza::Error::Cancel,
                                     QXmppStanza::Error::RecipientUnavailable);
            response.setError(error);
            server()->sendPacket(response);
        }
        return true;
    } else if (element.tagName() == "iq") {
        QXmppIq iq;
        iq.parse(element);
        if ((iq.type() == QXmppIq::Result) || (iq.type() == QXmppIq::Error)) {
            handlePingResult(iq);
            return true;
        }

        if (QXmppVCardIq::isVCard(element)) {
            return handleRequestVCard(element);
            // vCard request

        }

        if (QXmppDiscoveryIq::isDiscoveryIq(element)) {
            QXmppDiscoveryIq receivedIq;
            receivedIq.parse(element);
            if (receivedIq.type() == QXmppIq::Get) {
                return handleDiscoveryGet(receivedIq);
            }
            if (receivedIq.type() == QXmppIq::Result) {
                return handleDiscoveryResult(receivedIq);
            }
        } else if (QXmppPingIq::isPingIq(element)) {
            QXmppPingIq receivedIq;
            receivedIq.parse(element);
            if (receivedIq.type() == QXmppIq::Result) {
                handlePingResult(receivedIq);
                return true;
            }
        }
    } else if (element.tagName() == "presence") {
        QXmppPresence presenceStanza;
        presenceStanza.parse(element);
        if (presenceStanza.isMucSupported()) {
            // XEP-0045 / Enter a room
            return handleXmppUserJoinMuc(presenceStanza);
        }
    }

    return false;
}

bool TelegramExtension::handleRequestVCard(const QDomElement &element)
{
    // <iq xmlns="jabber:server" from="test5@127.0.0.3/user-PC" id="aac0a" to="123456789@127.0.0.1" type="get">
    //     <vCard xmlns="vcard-temp"/>
    // </iq>

    QXmppVCardIq request;
    request.parse(element);
    const AbstractUser *targetUser = xmpp()->getTelegramUser(request.to());

    // <iq id='v1'
    //     to='stpeter@jabber.org/roundabout'
    //     type='result'>
    //   <vCard xmlns='vcard-temp'>
    //     <FN>Peter Saint-Andre</FN>
    //     <N>
    //       <FAMILY>Saint-Andre</FAMILY>
    //       <GIVEN>Peter</GIVEN>
    //       <MIDDLE/>
    //     </N>
    //     <NICKNAME>stpeter</NICKNAME>
    //   </vCard>
    // </iq>

    QXmppVCardIq response(request.from());
    response.setType(QXmppIq::Result);
    response.setId(request.id());
    response.setFrom(request.to());
    if (!targetUser->userName().isEmpty()) {
        response.setNickName(targetUser->userName());
    }
    response.setFirstName(targetUser->firstName());
    QXmppVCardPhone phone;
    phone.setNumber(targetUser->phoneNumber());
    phone.setType(QXmppVCardPhone::Messaging);
    response.setPhones({ phone });

    ImageDescriptor descriptor = targetUser->getCurrentImage();
    if (descriptor.isValid()) {
        // has avatar
        for (const ImageSizeDescriptor &size : descriptor.sizes) {
            if (!size.bytes.isEmpty()) {
                response.setPhoto(size.bytes);
                response.setPhotoType(QLatin1String("image/png"));
                break;
            }
        }
    }

    server()->sendPacket(response);
    return true;
}

bool TelegramExtension::handleXmppUserJoinMuc(const QXmppPresence &stanza)
{
    GroupChat *muc = xmpp()->getTelegramChat(stanza.to());
    if (!muc) {
        return false;
    }

    QString invitedUserMucJid = stanza.to();
    QString fromJid = stanza.from();
    QString mucJid = xmpp()->getBareJid(muc->toPeer());

    XmppUser *fromUser = xmpp()->ensureUser(stanza.from());

    // Get the room name from 'to'
    // Check if the room does not exist
    // Broadcast the presence

    // TODO: status code, https://xmpp.org/extensions/xep-0045.html#enter-nonanon
    // Code 100 for "any occupant is allowed to see the user's full JID"
    // Code 170 for "room logging is enabled"
    // Maybe code 172 for "the room is non-anonymous"

    for (const ChatMember &member : muc->members()) {
        // Maybe it's OK to send presence to the joined user to inform about the role?
        if (member.userId == fromUser->userId()) {
            continue;
        }

        QXmppPresence memberPresence;
        memberPresence.setFrom(invitedUserMucJid);
        memberPresence.setTo(fromJid);
        QXmppMucItem mucItem;
        mucItem.setJid(xmpp()->getUserBareJid(member.userId));
        switch (member.role) {
        case ChatMember::Role::Admin:
            mucItem.setAffiliation(QXmppMucItem::AdminAffiliation);
            mucItem.setRole(QXmppMucItem::ModeratorRole);
            break;
        case ChatMember::Role::Creator:
            mucItem.setAffiliation(QXmppMucItem::OwnerAffiliation);
            mucItem.setRole(QXmppMucItem::ModeratorRole);
            break;
        case ChatMember::Role::User:
            mucItem.setAffiliation(QXmppMucItem::MemberAffiliation);
            mucItem.setRole(QXmppMucItem::ParticipantRole);
            break;
        case ChatMember::Role::Invalid:
            continue; // Omit the invalid member
            break;
        }
        memberPresence.setMucItem(mucItem);
        server()->sendPacket(memberPresence);
    }

    return false;
}

bool TelegramExtension::handleMessage(const QXmppMessage &stanza)
{
    //  <message xmlns="jabber:server" to="telegram@127.0.0.2" id="aabfa" from="qxmpp.test1@127.0.0.3/localhost">
    //     <subject xmlns="jabber:server"/>
    //     <body xmlns="jabber:server">asdfasdf</body>
    //     <nick xmlns="http://jabber.org/protocol/nick">qxmpp.test1</nick>
    // </message>

    XmppUser *fromUser = xmpp()->ensureUser(stanza.from());
    const Peer targetPeer = xmpp()->getPeerFromJid(stanza.to());
    if (!targetPeer.isValid()) {
        qWarning() << "Unable to delivery message: peer" << stanza.to() << "not found";
        return false;
    }

    xmpp()->sendMessageFromXmpp(fromUser, targetPeer, stanza.body());

    return true;
}

bool TelegramExtension::handleDiscoveryGet(const QXmppDiscoveryIq &stanza)
{
    if (stanza.queryType() == QXmppDiscoveryIq::InfoQuery) {
        if (stanza.queryNode() == QLatin1String("x-roomuser-item")) {

        }
    }

    return false;
}

bool TelegramExtension::handleDiscoveryResult(const QXmppDiscoveryIq &stanza)
{
    // <iq type='result'
    //     from='juliet@capulet.com'
    //     to='shakespeare.lit'
    //     id='info2'>
    //   <query xmlns='http://jabber.org/protocol/disco#info'>
    //     <identity category='account' type='registered'/>
    //     <feature var='http://jabber.org/protocol/disco#info'/>
    //   </query>
    // </iq>

    const QString requestId = stanza.id();
    PendingVariant *operation = xmpp()->takeRequest(requestId);
    if (!operation) {
        qWarning() << __func__ << "Unexpected discovery id";
        return false;
    }

    QList<QXmppDiscoveryIq::Identity> identities = stanza.identities();
    if (identities.isEmpty()) {
        // Contact not found
        operation->setFinished();
        return true;
    }

    if (identities.count() != 1) {
        operation->setFinishedWithTextError(QLatin1String("Unexpected result"));
        return true;
    }

    if (identities.constFirst().category() != "account") {
        operation->setFinishedWithTextError(QLatin1String("Unexpected result"));
        return true;
    }

    // Seems to be a valid result
    XmppUser *user = xmpp()->ensureUser(stanza.from());
    PeerList result = {
        Peer::fromUserId(user->userId()),
    };
    operation->setResult(QVariant::fromValue(result));

    return true;
}

bool TelegramExtension::handlePingResult(const QXmppIq &stanza)
{
    const QString requestId = stanza.id();
    PendingVariant *operation = xmpp()->takeRequest(requestId);

    if (!operation) {
        qWarning() << __func__ << "Unexpected discovery id";
        return false;
    }

    // Seems to be a valid result
    XmppUser *user = xmpp()->ensureUser(stanza.from());
    PeerList result = {
        Peer::fromUserId(user->userId()),
    };
    operation->setResult(QVariant::fromValue(result));

    return true;
}

} // Server

} // Telegram
