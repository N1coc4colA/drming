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
    void loadClients() override;

    void deleteServerCert(const QString &fileName) override;
    void deleteClient(const QString &name) override;

    int addServerCert() override;
    int addClientCert(const QString &name) override;
    int addClientKey(const QString &name) override;

    QStringList validClientEntries() override;

    QString serverCertsPath() override;
    QString clientPath() override;
    QString clientCertName() override;
    QString clientKeyName() override;

    bool updateClientEntry(const QVariantMap &map) override;

    QPair<QSslCertificate, QSslKey> clientData(const QString &name) override;
    QList<QSslCertificate> trustedCerts() override;

private:
    bool createClientPathStorage(const QString &name);
    bool copyFile(const QString &title, const QString &filter, const QString &dst);
    bool isClientEntryValid(const QString &name);

    static constexpr auto folderName = "drming";
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
