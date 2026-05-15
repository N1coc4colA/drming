#include "dispsetup.h"

#include <cerrno>
#include <cstring>
#include <iostream>

#include <unistd.h>
#include <xf86drmMode.h>

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QThread>

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
        return false;
    }

    return f.write(value) == value.size();
}

bool makeSymlink(const QString &linkPath, const QString &target)
{
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const QByteArray link = linkPath.toLocal8Bit();
    const QByteArray tgt = target.toLocal8Bit();
    if (::symlink(tgt.constData(), link.constData()) != 0) {
        std::cout << ">> symlink(" << tgt.constData() << ", " << link.constData() << ") failed: " << ::strerror(errno) << "\n";
        return false;
    }

    return true;
}

QByteArray buildSyntheticEdid()
{
    const uint16_t pixelClock10kHz = 2167;
    const uint16_t hActive = 1024, hBlank = 320;
    const uint16_t vActive = 768, vBlank = 38;
    const uint8_t hSyncOff = 24, hSyncW = 136;
    const uint8_t vSyncOff = 3, vSyncW = 6;

    unsigned char edid[128] = {};

    edid[0] = 0x00;
    edid[1] = 0xFF;
    edid[2] = 0xFF;
    edid[3] = 0xFF;
    edid[4] = 0xFF;
    edid[5] = 0xFF;
    edid[6] = 0xFF;
    edid[7] = 0x00;

    edid[8] = 0x55;
    edid[9] = 0x33; // Manufacturer "VRT"
    edid[10] = 0x00;
    edid[11] = 0x00; // Product code
    edid[12] = 0x00;
    edid[13] = 0x00;
    edid[14] = 0x00;
    edid[15] = 0x00;
    edid[16] = 0x01;
    edid[17] = 0x20; // Week 1, year 2022
    edid[18] = 0x01;
    edid[19] = 0x03; // EDID 1.3
    edid[20] = 0x80; // Digital input
    edid[21] = 0x00;
    edid[22] = 0x00; // No physical size
    edid[23] = 0x78; // Gamma 2.2
    edid[24] = 0x0A; // Feature support

    for (int i = 25; i < 35; ++i) {
        edid[i] = 0x00; // Chromaticity
    }

    edid[35] = 0x00;
    edid[36] = 0x00;
    edid[37] = 0x00; // Established timings

    for (int i = 38; i < 54; i += 2) {
        edid[i] = 0x01;
        edid[i + 1] = 0x01;
    } // Standard timings unused

    // Descriptor 1: detailed timing 1024x768 @ 20Hz
    edid[54] = (pixelClock10kHz) & 0xFF;
    edid[55] = (pixelClock10kHz >> 8) & 0xFF;
    edid[56] = hActive & 0xFF;
    edid[57] = hBlank & 0xFF;
    edid[58] = ((hActive >> 4) & 0xF0) | ((hBlank >> 8) & 0x0F);
    edid[59] = vActive & 0xFF;
    edid[60] = vBlank & 0xFF;
    edid[61] = ((vActive >> 4) & 0xF0) | ((vBlank >> 8) & 0x0F);
    edid[62] = hSyncOff;
    edid[63] = hSyncW;
    edid[64] = ((vSyncOff & 0xF) << 4) | (vSyncW & 0xF);
    edid[65] = 0x00;
    edid[66] = 0x00;
    edid[67] = 0x00;
    edid[68] = 0x00; // No physical size
    edid[69] = 0x00;
    edid[70] = 0x00; // No border
    edid[71] = 0x18; // +hsync +vsync

    // Descriptor 2: monitor name
    edid[72] = 0x00;
    edid[73] = 0x00;
    edid[74] = 0x00;
    edid[75] = 0xFC;
    edid[76] = 0x00;
    const char *name = "VirtDisp\n   ";
    for (int i = 0; i < 13; ++i) {
        edid[77 + i] = static_cast<unsigned char>(name[i]);
    }

    // Descriptors 3 & 4: dummy
    edid[90] = 0x00;
    edid[91] = 0x00;
    edid[92] = 0x00;
    edid[93] = 0x10;
    edid[94] = 0x00;
    edid[108] = 0x00;
    edid[109] = 0x00;
    edid[110] = 0x00;
    edid[111] = 0x10;
    edid[112] = 0x00;

    edid[126] = 0x00; // No extensions

    int sum = 0;
    for (int i = 0; i < 127; ++i) {
        sum += edid[i];
    }

    edid[127] = static_cast<unsigned char>((256 - (sum % 256)) % 256);

    return QByteArray(reinterpret_cast<const char *>(edid), 128);
}

} // namespace

bool isConfigfsMounted()
{
    QFile f("/proc/mounts");
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return false;
    }

    const QByteArray m = f.readAll();
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
    for (const QString &name : {QString("/proc/config.gz"), QString("/boot/config-") + QSysInfo::kernelVersion()}) {
        QFile f(name);
        if (!f.open(QIODevice::ReadOnly)) {
            continue;
        }

        QByteArray data = f.readAll();
        if (data.startsWith("\x1f\x8b")) {
            data = qUncompress(data);
        }

        if (data.contains("CONFIG_DRM_VKMS=y") || data.contains("CONFIG_DRM_VKMS=m")) {
            return true;
        }
    }

    return false;
}

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
    const std::array<bool (DispSetup::*)(), 14> funcs{
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
        std::cerr << "Failed to enable VKMS instance.\n";
        return;
    }

    // Give the compositor time to detect the hotplug
    QThread::msleep(800);

    m_virtualConnectorName = findVirtualConnectorName();
    m_setup = !m_virtualConnectorName.isEmpty();

    if (!m_setup) {
        std::cerr << "VKMS device enabled but no Virtual connector found in /sys/class/drm\n";
    }
}

bool DispSetup::makeEnvChecks()
{
    if (!isConfigfsMounted()) {
        std::cerr << "configfs is not mounted.\n";
        return false;
    }

    if (!isVkmsModuleLoaded()) {
        std::cerr << "VKMS kernel module is not loaded.\n";
        return false;
    }

    if (!isVkmsConfigEnabled()) {
        std::cerr << "VKMS is not enabled in kernel config.\n";
        return false;
    }

    return true;
}

bool DispSetup::makeVkmsInstance()
{
    if (!QDir().mkpath(m_basePath)) {
        std::cerr << "Failed to create VKMS instance: " << qPrintable(m_basePath) << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makeCrtc()
{
    if (!QDir().mkpath(m_crtcPath)) {
        std::cerr << "Failed to create CRTC: " << qPrintable(m_crtcPath) << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makeEncoder()
{
    if (!QDir().mkpath(m_encoderPath)) {
        std::cerr << "Failed to create encoder: " << qPrintable(m_encoderPath) << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makePrimaryPlane()
{
    if (!QDir().mkpath(m_primaryPlanePath)) {
        std::cerr << "Failed to create primary plane: " << qPrintable(m_primaryPlanePath) << '\n';
        return false;
    }

    return true;
}

bool DispSetup::makeCursorPlane()
{
    if (!QDir().mkpath(m_cursorPlanePath)) {
        std::cerr << "Failed to create cursor plane: " << qPrintable(m_cursorPlanePath) << '\n';
        return false;
    }

    return true;
}

bool DispSetup::setPrimaryPlaneType()
{
    // DRM_PLANE_TYPE_PRIMARY = 1
    if (!writeFile(m_primaryPlanePath + "/type", QString::number(DRM_PLANE_TYPE_PRIMARY).toLocal8Bit())) {
        std::cerr << "Failed to set plane type to primary.\n";
        return false;
    }

    return true;
}

bool DispSetup::setCursorPlaneType()
{
    if (!writeFile(m_cursorPlanePath + "/type", QString::number(DRM_PLANE_TYPE_CURSOR).toLocal8Bit())) {
        std::cerr << "Failed to set plane type to cursor.\n";
        return false;
    }

    return true;
}

bool DispSetup::linkPrimaryPlaneToCrtc()
{
    const QString linkPath = m_primaryPlanePath + "/possible_crtcs/crtc0";
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const QByteArray link = linkPath.toLocal8Bit();
    const QByteArray tgt = m_crtcPath.toLocal8Bit();
    if (::symlink(tgt.constData(), link.constData()) != 0) {
        std::cerr << "Failed to symlink primary plane → crtc: " << qPrintable(linkPath) << " (" << ::strerror(errno) << ")\n";
        return false;
    }

    return true;
}

bool DispSetup::linkCursorPlaneToCrtc()
{
    const QString linkPath = m_cursorPlanePath + "/possible_crtcs/crtc0";
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const QByteArray link = linkPath.toLocal8Bit();
    const QByteArray tgt = m_crtcPath.toLocal8Bit();
    if (::symlink(tgt.constData(), link.constData()) != 0) {
        std::cerr << "Failed to symlink cursor plane → crtc: " << qPrintable(linkPath) << " (" << ::strerror(errno) << ")\n";
        return false;
    }

    return true;
}

bool DispSetup::linkEncoderToCrtc()
{
    const QString linkPath = m_encoderPath + "/possible_crtcs/crtc0";
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const QByteArray link = linkPath.toLocal8Bit();
    const QByteArray tgt = m_crtcPath.toLocal8Bit();
    if (::symlink(tgt.constData(), link.constData()) != 0) {
        std::cerr << "Failed to symlink encoder → crtc: " << qPrintable(linkPath) << " (" << ::strerror(errno) << ")\n";
        return false;
    }

    return true;
}

bool DispSetup::linkConnectorToEncoder()
{
    const QString linkPath = m_connectorPath + "/possible_encoders/encoder0";
    if (QFileInfo::exists(linkPath)) {
        return true;
    }

    const QByteArray link = linkPath.toLocal8Bit();
    const QByteArray tgt = m_encoderPath.toLocal8Bit();
    if (::symlink(tgt.constData(), link.constData()) != 0) {
        std::cerr << "Failed to symlink connector → encoder: " << qPrintable(linkPath) << " (" << ::strerror(errno) << ")\n";
        return false;
    }

    return true;
}

bool DispSetup::makeConnector()
{
    if (!QDir().mkpath(m_connectorPath)) {
        std::cerr << "Failed to create connector: " << qPrintable(m_connectorPath) << '\n';
        return false;
    }

    return true;
}

bool DispSetup::writeEdidAndEnable()
{
    const QString statusPath = m_connectorPath + "/status";
    writeFile(statusPath, "0");
    if (!writeFile(statusPath, "1")) {
        std::cerr << "Failed to set connector status.\n";
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

        QFile status(drmDir.filePath(entry) + QStringLiteral("/status"));
        if (!status.open(QIODevice::ReadOnly)) {
            continue;
        }

        if (status.readAll().trimmed() == "connected") {
            return entry;
        }
    }

    return {};
}
