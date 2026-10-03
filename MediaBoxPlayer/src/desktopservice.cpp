#include "desktopservice.h"

#include <QCoreApplication>
#include <QDebug>
#include <QMetaObject>
#include <QSocketNotifier>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <utility>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <mutex>
#elif defined(Q_OS_UNIX)
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#endif

namespace MediaBox {
namespace {

#ifdef Q_OS_WIN
constexpr wchar_t serviceName[] = L"MediaBoxPlayer";

struct WindowsServiceContext {
    int argc;
    char **argv;
    std::function<int(int, char **)> run;
    SERVICE_STATUS_HANDLE handle = nullptr;
    SERVICE_STATUS status{};
    std::mutex statusMutex;
    std::mutex applicationMutex;
    QCoreApplication *application = nullptr;
    std::atomic_bool stopRequested = false;
    bool startupFailure = false;
    int exitCode = EXIT_FAILURE;
};

WindowsServiceContext *serviceContext = nullptr;

// Caller holds statusMutex, which also orders STOP against notifyReady().
bool publishStatus(WindowsServiceContext &context, DWORD state, DWORD exitCode = NO_ERROR)
{
    context.status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    context.status.dwCurrentState = state;
    context.status.dwControlsAccepted = state == SERVICE_RUNNING
        ? SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN : 0;
    context.status.dwWin32ExitCode = exitCode == NO_ERROR ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR;
    context.status.dwServiceSpecificExitCode = exitCode;
    context.status.dwCheckPoint = (state == SERVICE_START_PENDING || state == SERVICE_STOP_PENDING) ? 1 : 0;
    context.status.dwWaitHint = context.status.dwCheckPoint ? 30000 : 0;
    return SetServiceStatus(context.handle, &context.status) != FALSE;
}

DWORD WINAPI serviceControl(DWORD control, DWORD, LPVOID, LPVOID userData)
{
    auto &context = *static_cast<WindowsServiceContext *>(userData);
    if (control == SERVICE_CONTROL_INTERROGATE) {
        const std::lock_guard lock(context.statusMutex);
        SetServiceStatus(context.handle, &context.status);
        return NO_ERROR;
    }
    if (control != SERVICE_CONTROL_STOP && control != SERVICE_CONTROL_SHUTDOWN)
        return ERROR_CALL_NOT_IMPLEMENTED;

    {
        const std::lock_guard lock(context.statusMutex);
        if (context.stopRequested.exchange(true))
            return NO_ERROR;
        publishStatus(context, SERVICE_STOP_PENDING);
    }
    {
        // The lifecycle destructor removes this pointer under the same lock.
        // A queued call also handles STOP between notifyReady() and exec().
        const std::lock_guard lock(context.applicationMutex);
        if (context.application)
            QMetaObject::invokeMethod(context.application, &QCoreApplication::quit, Qt::QueuedConnection);
    }
    return NO_ERROR;
}

void WINAPI serviceMain(DWORD, LPWSTR *)
{
    auto &context = *serviceContext;
    context.handle = RegisterServiceCtrlHandlerExW(serviceName, serviceControl, &context);
    if (!context.handle)
        return;
    {
        const std::lock_guard lock(context.statusMutex);
        if (!publishStatus(context, SERVICE_START_PENDING))
            return;
    }
    try {
        context.exitCode = context.run(context.argc, context.argv);
        if (context.startupFailure && context.exitCode == EXIT_SUCCESS)
            context.exitCode = EXIT_FAILURE;
    } catch (const std::exception &exception) {
        qCritical() << "MediaBoxPlayer service failed:" << exception.what();
        context.exitCode = EXIT_FAILURE;
    } catch (...) {
        qCritical() << "MediaBoxPlayer service failed with an unknown exception";
        context.exitCode = EXIT_FAILURE;
    }
    const std::lock_guard lock(context.statusMutex);
    publishStatus(context, SERVICE_STOPPED, static_cast<DWORD>(context.exitCode));
}
#elif defined(Q_OS_UNIX)
volatile sig_atomic_t shutdownWriteFd = -1;

extern "C" void shutdownSignalHandler(int signalNumber)
{
    const int savedErrno = errno;
    const int descriptor = shutdownWriteFd;
    if (descriptor >= 0) {
        const unsigned char value = static_cast<unsigned char>(signalNumber);
        // write(2) is async-signal-safe; a full pipe already contains a quit request.
        const auto ignored = ::write(descriptor, &value, sizeof(value));
        (void)ignored;
    }
    errno = savedErrno;
}
#endif

} // namespace

struct DesktopServiceLifecycle::Private {
    bool installed = false;
#if defined(Q_OS_UNIX)
    int readFd = -1;
    int writeFd = -1;
    struct sigaction oldTerm{};
    struct sigaction oldInt{};
    QSocketNotifier *notifier = nullptr;
#endif
};

int runWindowsService(int argc, char **argv, std::function<int(int, char **)> run)
{
#ifdef Q_OS_WIN
    WindowsServiceContext context{argc, argv, std::move(run)};
    serviceContext = &context;
    SERVICE_TABLE_ENTRYW table[] = {
        {const_cast<LPWSTR>(serviceName), serviceMain},
        {nullptr, nullptr}
    };
    if (!StartServiceCtrlDispatcherW(table)) {
        qCritical() << "Cannot connect MediaBoxPlayer to Windows Service Control Manager."
                    << "Use --service only for a registered Windows service. Error:" << GetLastError();
        serviceContext = nullptr;
        return EXIT_FAILURE;
    }
    serviceContext = nullptr;
    return context.exitCode;
#else
    return run(argc, argv);
#endif
}

DesktopServiceLifecycle::DesktopServiceLifecycle(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
}

DesktopServiceLifecycle::~DesktopServiceLifecycle()
{
#ifdef Q_OS_WIN
    if (d->installed && serviceContext) {
        const std::lock_guard lock(serviceContext->applicationMutex);
        serviceContext->application = nullptr;
    }
#elif defined(Q_OS_UNIX)
    if (d->installed) {
        shutdownWriteFd = -1;
        ::sigaction(SIGTERM, &d->oldTerm, nullptr);
        ::sigaction(SIGINT, &d->oldInt, nullptr);
    }
    delete d->notifier;
    if (d->readFd >= 0)
        ::close(d->readFd);
    if (d->writeFd >= 0)
        ::close(d->writeFd);
#endif
}

bool DesktopServiceLifecycle::install(QString *error)
{
    if (error)
        error->clear();
    if (d->installed)
        return true;
    if (!QCoreApplication::instance()) {
        if (error)
            *error = QStringLiteral("Desktop service lifecycle requires QCoreApplication");
        return false;
    }
#ifdef Q_OS_WIN
    if (serviceContext) {
        const std::lock_guard lock(serviceContext->applicationMutex);
        serviceContext->application = QCoreApplication::instance();
    }
#elif defined(Q_OS_UNIX)
    if (shutdownWriteFd >= 0) {
        if (error)
            *error = QStringLiteral("Shutdown signal handlers are already installed");
        return false;
    }
    int descriptors[2];
    if (::pipe(descriptors) != 0) {
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(errno));
        return false;
    }
    for (const int descriptor : descriptors) {
        if (::fcntl(descriptor, F_SETFL, O_NONBLOCK) == -1
            || ::fcntl(descriptor, F_SETFD, FD_CLOEXEC) == -1) {
            const int savedErrno = errno;
            ::close(descriptors[0]);
            ::close(descriptors[1]);
            if (error)
                *error = QString::fromLocal8Bit(std::strerror(savedErrno));
            return false;
        }
    }
    struct sigaction action{};
    action.sa_handler = shutdownSignalHandler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;
    shutdownWriteFd = descriptors[1];
    if (::sigaction(SIGTERM, &action, &d->oldTerm) != 0) {
        const int savedErrno = errno;
        shutdownWriteFd = -1;
        ::close(descriptors[0]);
        ::close(descriptors[1]);
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(savedErrno));
        return false;
    }
    if (::sigaction(SIGINT, &action, &d->oldInt) != 0) {
        const int savedErrno = errno;
        shutdownWriteFd = -1;
        ::sigaction(SIGTERM, &d->oldTerm, nullptr);
        ::close(descriptors[0]);
        ::close(descriptors[1]);
        if (error)
            *error = QString::fromLocal8Bit(std::strerror(savedErrno));
        return false;
    }
    d->readFd = descriptors[0];
    d->writeFd = descriptors[1];
    d->notifier = new QSocketNotifier(d->readFd, QSocketNotifier::Read, this);
    connect(d->notifier, &QSocketNotifier::activated, this, [this] {
        char buffer[64];
        while (::read(d->readFd, buffer, sizeof(buffer)) > 0) {}
        QCoreApplication::quit();
    });
#endif
    d->installed = true;
    return true;
}

bool DesktopServiceLifecycle::notifyReady()
{
#ifdef Q_OS_WIN
    if (serviceContext) {
        const std::lock_guard lock(serviceContext->statusMutex);
        if (serviceContext->stopRequested.load())
            return false;
        if (!publishStatus(*serviceContext, SERVICE_RUNNING)) {
            serviceContext->startupFailure = true;
            qCritical() << "Cannot publish Windows service readiness:" << GetLastError();
            return false;
        }
    }
#endif
    return true;
}

} // namespace MediaBox
