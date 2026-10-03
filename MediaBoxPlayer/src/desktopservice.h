#pragma once

#include <QObject>
#include <QString>

#include <functional>
#include <memory>

namespace MediaBox {

// Call before constructing QCoreApplication, and only for an SCM launch (--service).
// On other platforms this forwards directly to run.
int runWindowsService(int argc, char **argv, std::function<int(int, char **)> run);

// Construct after QCoreApplication, on its thread, and retain until exec() returns.
class DesktopServiceLifecycle final : public QObject
{
public:
    explicit DesktopServiceLifecycle(QObject *parent = nullptr);
    ~DesktopServiceLifecycle() override;

    bool install(QString *error = nullptr);

    // Call once the audio engine and command listener have successfully started.
    // False means shutdown was already requested: do not enter application.exec().
    bool notifyReady();

private:
    struct Private;
    std::unique_ptr<Private> d;
};

} // namespace MediaBox
