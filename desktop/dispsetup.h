#ifndef DISPSETUP_H
#define DISPSETUP_H

#include <QString>

class DispSetup
{
public:
    explicit DispSetup(const QString &instanceName);

    inline bool isSetup() const { return m_setup; }
    inline const QString &virtualConnectorName() const { return m_virtualConnectorName; }
    inline const QString &basePath() const { return m_basePath; }

private:
    bool makeEnvChecks();
    bool makeVkmsInstance();
    bool makeCrtc();
    bool makeEncoder();
    bool makePrimaryPlane();
    bool makeCursorPlane();
    bool setPrimaryPlaneType();
    bool setCursorPlaneType();
    bool linkPrimaryPlaneToCrtc();
    bool linkCursorPlaneToCrtc();
    bool linkEncoderToCrtc();
    bool makeConnector();
    bool linkConnectorToEncoder();
    bool writeEdidAndEnable();

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

bool isConfigfsMounted();
bool isVkmsModuleLoaded();
bool isVkmsConfigEnabled();

#endif
