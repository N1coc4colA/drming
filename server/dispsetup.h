#ifndef DISPSETUP_H
#define DISPSETUP_H

#include <QString>

/**
 * Creates and configures one VKMS connector through the kernel configfs API.
 *
 * Construction performs the setup transaction; isSetup reports whether the
 * connector is ready for DisplayReader to capture.
 */
class DispSetup
{
public:
    explicit DispSetup(const QString &instanceName);
    ~DispSetup();

    /// Closes and disconnects the VKMS display, cleaning up configfs entries.
    void close();

    [[nodiscard]] bool isSetup() const
    {
        return m_setup;
    }
    /// Returns the name of the created VKMS virtual connector.
    [[nodiscard]] const QString &virtualConnectorName() const
    {
        return m_virtualConnectorName;
    }
    /// Returns the configfs base path used for VKMS configuration.
    [[nodiscard]] const QString &basePath() const
    {
        return m_basePath;
    }

private:
    [[nodiscard]] bool makeEnvChecks() const;
    [[nodiscard]] bool makeVkmsInstance() const;
    [[nodiscard]] bool makeCrtc() const;
    [[nodiscard]] bool makeEncoder() const;
    [[nodiscard]] bool makePrimaryPlane() const;
    [[nodiscard]] bool makeCursorPlane() const;
    [[nodiscard]] bool setPrimaryPlaneType() const;
    [[nodiscard]] bool setCursorPlaneType() const;
    [[nodiscard]] bool linkPrimaryPlaneToCrtc() const;
    [[nodiscard]] bool linkCursorPlaneToCrtc() const;
    [[nodiscard]] bool linkEncoderToCrtc() const;
    [[nodiscard]] bool makeConnector() const;
    [[nodiscard]] bool linkConnectorToEncoder() const;
    [[nodiscard]] bool writeEdidAndEnable() const;

    [[nodiscard]] static bool linkPlaneToCrtc(const QString &planePath, const QString &crtcPath);

    /// Locates the virtual connector name created by the VKMS device.
    [[nodiscard]] QString findVirtualConnectorName() const;

    QString m_instanceName;
    QString m_basePath;
    QString m_crtcPath;
    QString m_encoderPath;
    QString m_primaryPlanePath;
    QString m_cursorPlanePath;
    QString m_connectorPath;
    QString m_enabledPath;
    QString m_virtualConnectorName;

    bool m_setup = false;
};

#endif
