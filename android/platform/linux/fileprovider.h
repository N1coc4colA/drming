#ifndef FILEPROVIDERPLATFORM_H
#define FILEPROVIDERPLATFORM_H

#include "../../fileprovider.h"

namespace Platform {

class FileProvider : public ::FileProvider
{
    Q_OBJECT

public:
    explicit FileProvider(QObject *parent);

    void loadServerCerts() override;
    void loadClientCerts() override;

    void deleteServerCert(const QString &fileName) override;
    void deleteClientCert(const QString &fileName) override;

    void addServerCert() override;
    void addClientCert() override;

    QString serverCertsPath() override;
    QString clientCertsPath() override;

    static constexpr auto folderName = "drming";
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
