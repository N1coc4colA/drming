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
    [[nodiscard]] const QString &virtualConnectorName() const
    {
        return m_virtualConnectorName;
    }
    [[nodiscard]] const QString &basePath() const
    {
        return m_basePath;
    }

private:
    bool makeEnvChecks() const;
    bool makeVkmsInstance() const;
    bool makeCrtc() const;
    bool makeEncoder() const;
    bool makePrimaryPlane() const;
    bool makeCursorPlane() const;
    bool setPrimaryPlaneType() const;
    bool setCursorPlaneType() const;
    bool linkPrimaryPlaneToCrtc() const;
    bool linkCursorPlaneToCrtc() const;
    bool linkEncoderToCrtc() const;
    bool makeConnector() const;
    bool linkConnectorToEncoder() const;
    bool writeEdidAndEnable() const;

    [[nodiscard]] bool linkPlaneToCrtc(const QString &planePath, const QString &crtcPath) const;

    QString findVirtualConnectorName() const;

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
