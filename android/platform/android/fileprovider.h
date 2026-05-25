#ifndef FILEPROVIDERPLATFORM_H
#define FILEPROVIDERPLATFORM_H

#include "../../fileprovider.h"

namespace Platform {

class FileProvider : public ::FileProvider
{
    Q_OBJECT

public:
    explicit FileProvider(QObject *parent);

    QList<QString> loadServerCerts() override;
    QList<QString> loadClientCerts() override;

    void deleteServerCert(const QString &file) override;
    void deleteClientCert(const QString &file) override;

    void addServerCert() override;
    void addClientCerts() override;
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
