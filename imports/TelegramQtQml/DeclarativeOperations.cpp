#include "DeclarativeOperations.hpp"
#include "DeclarativeClient.hpp"

#include <QTimer>
#include <QDebug>

#include "../../TelegramQt/Client.hpp"

namespace Telegram {

using namespace Client;

DeclarativeOperation::DeclarativeOperation(QObject *parent) :
    QObject(parent)
{
}

DeclarativeClient *DeclarativeOperation::target() const
{
    return m_target;
}

bool DeclarativeOperation::succeed() const
{
    return m_succeed;
}

void DeclarativeOperation::start()
{
//    m_running = true;
    emit started();
    startEvent();
}

void DeclarativeOperation::setTarget(DeclarativeClient *target)
{
    m_target = target;
}

void DeclarativeOperation::startEvent()
{
}

void DeclarativeOperation::setPendingOperation(PendingOperation *op)
{
    connect(op, &PendingOperation::succeeded, this, &DeclarativeOperation::onOperationSucceed);
    connect(op, &PendingOperation::failed, this, &DeclarativeOperation::onOperationFailed);
}

void DeclarativeOperation::onOperationSucceed(PendingOperation *op)
{
    Q_UNUSED(op)
    emit succeeded();
    setSucceed(true);
    emit finished();
}

void DeclarativeOperation::onOperationFailed(PendingOperation *op, const QVariantHash &details)
{
    Q_UNUSED(op)
    emit failed(details);
    setSucceed(false);
    emit finished();
}

void DeclarativeOperation::setSucceed(bool succeed)
{
    if (m_succeed == succeed) {
        return;
    }
    m_succeed = succeed;
    emit succeedChanged(succeed);
}

DeclarativeAuthOperation::DeclarativeAuthOperation(QObject *parent) :
    DeclarativeOperation(parent)
{
//    connect(this, &DeclarativeAuthOperation::statusChanged, [this](AuthStatus status) {
//        switch (status) {
//        case AuthCodeRequired:
//            emit authCodeRequired();
//            break;
//        case PasswordRequired:
//            setPasswordHint("My hint");
//            emit passwordRequired();
//            break;
//        default:
//            break;
//        }
    //    });
}

bool DeclarativeAuthOperation::isBusy() const
{
    return m_busy;
}

DeclarativeAuthOperation::AuthStatus DeclarativeAuthOperation::status() const
{
    return m_status;
}

QString DeclarativeAuthOperation::phoneNumber() const
{
    return m_phoneNumber;
}

QString DeclarativeAuthOperation::passwordHint() const
{
    return m_authOperation ? m_authOperation->passwordHint() : QString();
}

bool DeclarativeAuthOperation::hasRecovery() const
{
    return m_authOperation ? m_authOperation->hasRecovery() : false;
}

void DeclarativeAuthOperation::abort()
{
    m_authOperation->abort();
}

bool DeclarativeAuthOperation::submitAuthCode(const QString &code)
{
    if (!m_authOperation) {
        return false;
    }
    PendingOperation *op = m_authOperation->submitAuthCode(code);
    setBusy(true);
    connect(op, &PendingOperation::finished, this, &DeclarativeAuthOperation::unsetBusy);
    return true;
}

bool DeclarativeAuthOperation::submitPassword(const QString &password)
{
    if (!m_authOperation) {
        return false;
    }
    PendingOperation *op = m_authOperation->submitPassword(password);
    setBusy(true);
    connect(op, &PendingOperation::finished, this, &DeclarativeAuthOperation::unsetBusy);
    return true;
}

void DeclarativeAuthOperation::setPhoneNumber(const QString &phoneNumber)
{
    if (m_phoneNumber == phoneNumber) {
        return;
    }
    m_phoneNumber = phoneNumber;
    emit phoneNumberChanged();
}

bool DeclarativeAuthOperation::recovery()
{
    if (!m_authOperation || !m_authOperation->hasRecovery()) {
        return false;
    }
    m_authOperation->recovery();
    setBusy(true);
    return true;
}

void DeclarativeAuthOperation::setStatus(const AuthStatus status)
{
    qWarning() << Q_FUNC_INFO << status;
    if (m_status == status) {
        return;
    }
    m_status = status;
    emit statusChanged(status);
}

void DeclarativeAuthOperation::setBusy(bool busy)
{
    if (m_busy == busy) {
        return;
    }
    m_busy = busy;
    emit busyChanged(busy);

//    QTimer::singleShot(1100, this, &DeclarativeAuthOperation::unsetBusy);
}

void DeclarativeAuthOperation::unsetBusy()
{
    setBusy(false);
}

void DeclarativeAuthOperation::onPasswordRequired()
{
    setBusy(true);
    PendingOperation *op = m_authOperation->getPassword();
    connect(op, &PendingOperation::finished, this, &DeclarativeAuthOperation::unsetBusy);
    emit passwordRequired();
}

void DeclarativeAuthOperation::startEvent()
{
    m_target->syncSettings();
    m_authOperation = m_target->backend()->signIn();
    m_authOperation->setPhoneNumber(phoneNumber());
    setPendingOperation(m_authOperation);
    connect(m_authOperation, &PendingAuthOperation::phoneNumberRequired, this, &DeclarativeAuthOperation::phoneNumberRequired);
    connect(m_authOperation, &PendingAuthOperation::authCodeRequired, this, &DeclarativeAuthOperation::authCodeRequired);
    connect(m_authOperation, &PendingAuthOperation::passwordRequired, this, &DeclarativeAuthOperation::onPasswordRequired);
    connect(m_authOperation, &PendingAuthOperation::passwordCheckFailed, this, &DeclarativeAuthOperation::passwordCheckFailed);

    connect(m_authOperation, &PendingAuthOperation::passwordHintChanged, this, &DeclarativeAuthOperation::passwordHintChanged);
    connect(m_authOperation, &PendingAuthOperation::hasRecoveryChanged, this, &DeclarativeAuthOperation::hasRecoveryChanged);
}

bool DeclarativeAuthOperation::requestCall()
{
    if (!m_authOperation) {
        return false;
    }
    m_authOperation->requestCall();
    return true;
}

bool DeclarativeAuthOperation::requestSms()
{
    if (!m_authOperation) {
        return false;
    }
    m_authOperation->requestSms();
    return true;
}

} // Telegram