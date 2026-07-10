#include "dispsetup.h"

#include <cerrno>
#include <cstring>

#include <unistd.h>
#include <xf86drmMode.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QThread>

#include "../settings.h"

#include "utils.h"

// See https://docs.kernel.org/gpu/vkms.html

namespace {

QString configfsRootPath()
{
    if (QDir(QStringLiteral("/sys/kernel/config/vkms")).exists()) {
        return QStringLiteral("/sys/kernel/config");
    }

    if (QDir(QStringLiteral("/config/vkms")).exists()) {
        return QStringLiteral("/config");
    }

    return QStringLiteral("/sys/kernel/config");
}

bool writeFile(const QString &path, const QByteArray &value)
{
    QFile f(path);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        qCritical() << f.errorString();
        return false;
    }

    return f.write(value) == value.size();
}

bool isConfigfsMounted()
{
    QFile f("/proc/mounts");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const auto m = f.readAll();
    return m.contains("configfs") && (m.contains("/sys/kernel/config ") || m.contains("/config "));
}

bool isVkmsModuleLoaded()
{
    QFile f("/proc/modules");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    return f.readAll().contains("vkms ");
}

bool isVkmsConfigEnabled()
{
    for (const auto &name : {QString("/proc/config.gz"), QString("/boot/config-") + QSysInfo::kernelVersion()}) {
        QFile f(name);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }

        auto data = f.readAll();
        if (data.startsWith("\x1f\x8b")) {
            data = qUncompress(data);
        }

        if (data.contains("CONFIG_DRM_VKMS=y") || data.contains("CONFIG_DRM_VKMS=m")) {
            return true;
        }
    }

    return false;
}

bool loadVkmsModule()
{
    qInfo() << "Inserting vkms";

    QProcess process{};
    process.start("modprobe", {"vkms", "enable_cursor=1"});

    while (!process.waitForFinished()) {
    }

    return process.exitCode() == 0;
}

} // namespace

DispSetup::DispSetup(const QString &instanceName)
    : m_instanceName(instanceName)
    , m_basePath(configfsRootPath() + "/vkms/" + instanceName)
    , m_crtcPath(m_basePath + "/crtcs/crtc0")
    , m_encoderPath(m_basePath + "/encoders/encoder0")
    , m_primaryPlanePath(m_basePath + "/planes/plane0")
    , m_cursorPlanePath(m_basePath + "/planes/plane1")
    , m_connectorPath(m_basePath + "/connectors/connector0")
    , m_enabledPath(m_basePath + "/enabled")
{
    constexpr std::array<bool (DispSetup::*)() const, 14> funcs{
        &DispSetup::makeEnvChecks,
        &DispSetup::makeVkmsInstance,
        &DispSetup::makeCrtc,
        &DispSetup::makeEncoder,
        &DispSetup::makePrimaryPlane,
        &DispSetup::setPrimaryPlaneType,
        &DispSetup::makeCursorPlane,
        &DispSetup::setCursorPlaneType,
        &DispSetup::linkPrimaryPlaneToCrtc,
        &DispSetup::linkCursorPlaneToCrtc,
        &DispSetup::linkEncoderToCrtc,
        &DispSetup::makeConnector,
        &DispSetup::linkConnectorToEncoder,
        &DispSetup::writeEdidAndEnable,
    };

    for (const auto &fn : funcs) {
        if (!(this->*fn)()) {
            return;
        }
    }

    // Write enabled=1 to register the device with the kernel
    if (!writeFile(m_enabledPath, "1")) {
        qCritical() << "Failed to enable VKMS instance.";
        return;
    }

    // Give the compositor time to detect the hotplug
    QThread::msleep(800);

    m_virtualConnectorName = findVirtualConnectorName();
    m_setup = !m_virtualConnectorName.isEmpty();

    if (!m_setup) {
        qCritical() << "VKMS device enabled but no Virtual connector found in /sys/class/drm";
    }
}

bool DispSetup::makeEnvChecks() const
{
    if (!isConfigfsMounted()) {
        qCritical() << "configfs is not mounted.";
        return false;
    }

    if (!isVkmsModuleLoaded()) {
        if (!loadVkmsModule()) {
            qCritical() << "VKMS kernel module is not loaded. Its loading failed. You can load it with: modprobe vkms enable_cursor=1";
            return false;
        }
        // We need to wait for the module to load, it is async.
        // Otherwise, the program will continue too quickly and fail.
        sleep(2);
    }

    if (!isVkmsConfigEnabled()) {
        qCritical() << "VKMS is not enabled in kernel config, please enable it.";
        return false;
    }

    return true;
}

bool DispSetup::makeVkmsInstance() const
{
    if (!QDir().mkpath(m_basePath)) {
        qCritical() << "Failed to create VKMS instance: " << m_basePath << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makeCrtc() const
{
    if (!QDir().mkpath(m_crtcPath)) {
        qCritical() << "Failed to create CRTC: " << m_crtcPath << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makeEncoder() const
{
    if (!QDir().mkpath(m_encoderPath)) {
        qCritical() << "Failed to create encoder: " << m_encoderPath << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makePrimaryPlane() const
{
    if (!QDir().mkpath(m_primaryPlanePath)) {
        qCritical() << "Failed to create primary plane: " << m_primaryPlanePath << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makeCursorPlane() const
{
    if (!QDir().mkpath(m_cursorPlanePath)) {
        qCritical() << "Failed to create cursor plane: " << m_cursorPlanePath << '\n';
        return false;
    }

    return true;
}

bool DispSetup::setPrimaryPlaneType() const
{
    // DRM_PLANE_TYPE_PRIMARY = 1
    if (!writeFile(m_primaryPlanePath + "/type", QString::number(DRM_PLANE_TYPE_PRIMARY).toLocal8Bit())) {
        qCritical() << "Failed to set plane type to primary.";
        return false;
    }

    return true;
}

bool DispSetup::setCursorPlaneType() const
{
    if (!writeFile(m_cursorPlanePath + "/type", QString::number(DRM_PLANE_TYPE_CURSOR).toLocal8Bit())) {
        qCritical() << "Failed to set plane type to cursor.";
        return false;
    }

    return true;
}

bool DispSetup::linkPlaneToCrtc(const QString &planePath, const QString &crtcPath) const
{
    const auto linkDir = planePath + "/possible_crtcs";
    const auto linkPath = linkDir + "/crtc0";
    const auto link = linkPath.toLocal8Bit();
    const auto tgt = crtcPath.toLocal8Bit();

    // Wait for ConfigFS to show up.
    int i = 0;
    for (; i < Settings::maximumLPTries; i++) {
        msleep(500);

        if (QFileInfo::exists(linkPath)) {
            return true;
        }

        // Ensure parent directory exists before attempting symlink
        if (!QDir().mkpath(linkDir)) {
            if (i == Settings::maximumLPTries - 1) {
                qCritical() << "Failed to create parent directory for plane symlink";
                return false;
            }
            continue;
        }

        if (symlink(tgt.constData(), link.constData()) == 0) {
            return true;
        }
    }

    qCritical() << "Failed to symlink plane → crtc: " << linkPath << " (" << strerror(errno) << ")";
    return false;
}

bool DispSetup::linkPrimaryPlaneToCrtc() const
{
    return linkPlaneToCrtc(m_primaryPlanePath, m_crtcPath);
}

bool DispSetup::linkCursorPlaneToCrtc() const
{
    return linkPlaneToCrtc(m_cursorPlanePath, m_crtcPath);
}

bool DispSetup::linkEncoderToCrtc() const
{
    const auto linkPath = m_encoderPath + "/possible_crtcs/crtc0";
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const auto link = linkPath.toLocal8Bit();
    const auto tgt = m_crtcPath.toLocal8Bit();
    if (symlink(tgt.constData(), link.constData()) != 0) {
        qCritical() << "Failed to symlink encoder → crtc: " << linkPath << " (" << strerror(errno) << ")";
        return false;
    }

    return true;
}

bool DispSetup::linkConnectorToEncoder() const
{
    const auto linkPath = m_connectorPath + "/possible_encoders/encoder0";
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const auto link = linkPath.toLocal8Bit();
    const auto tgt = m_encoderPath.toLocal8Bit();
    if (symlink(tgt.constData(), link.constData()) != 0) {
        qCritical() << "Failed to symlink connector → encoder: " << linkPath << " (" << strerror(errno) << ")";
        return false;
    }

    return true;
}

bool DispSetup::makeConnector() const
{
    if (!QDir().mkpath(m_connectorPath)) {
        qCritical() << "Failed to create connector: " << m_connectorPath << '\n';
        return false;
    }

    return true;
}

bool DispSetup::writeEdidAndEnable() const
{
    const auto statusPath = m_connectorPath + "/status";

    if (!writeFile(statusPath, "1")) {
        qCritical() << "Failed to set connector status.";
        return false;
    }

    return true;
}

QString DispSetup::findVirtualConnectorName() const
{
    const QDir drmDir(QStringLiteral("/sys/class/drm"));
    const auto entries = drmDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot);

    for (const QString &entry : entries) {
        if (!entry.contains(QStringLiteral("-Virtual-"))) {
            continue;
        }

        // Check if connector is connected (just in case)
        QFile status(drmDir.filePath(entry) + QStringLiteral("/status"));
        if (!status.open(QIODevice::ReadOnly)) {
            continue;
        }

        if (status.readAll().trimmed() != "connected") {
            continue;
        }

        // Verify this connector belongs to our instance by checking the device symlink
        const QFileInfo deviceInfo(drmDir.filePath(entry));
        if (!deviceInfo.isSymLink()) {
            continue;
        }

        // Read the symlink target to get the device name
        const auto target = deviceInfo.symLinkTarget();

        // The symlink target should contain our instance name
        // e.g., "../../../drming_0" or "/sys/devices/faux/drming_0"
        if (target.contains(m_instanceName)) {
            return entry;
        }
    }

    qCritical() << "No matching virtual connector found for instance:" << m_instanceName;
    return {};
}
