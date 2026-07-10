#ifndef FILEPROVIDERPLATFORM_H
#define FILEPROVIDERPLATFORM_H

#include "../../fileprovider.h"

#include <QJniObject>

#include <functional>

template<typename T>
class JavaProxy;

namespace Platform {

class FileProvider : public ::FileProvider
{
    Q_OBJECT

    template<typename... Args>
    using Fn = std::function<Args...>;

public:
    explicit FileProvider(QObject *parent);

    void loadServerCerts() override;
    void loadClients() override;

    void deleteServerCert(const QString &file) override;
    void deleteClient(const QString &file) override;

    int addServerCert() override;
    int addClientCert(const QString &name) override;
    int addClientKey(const QString &name) override;

    QStringList validClientEntries() override;

    QPair<QSslCertificate, QSslKey> clientData(const QString &name) override;
    QList<QSslCertificate> trustedCerts() override;

    QString serverCertsPath() override;
    QString clientPath() override;
    QString clientCertName() override;
    QString clientKeyName() override;

    bool updateClientEntry(const QVariantMap &map) override;

private:
    QJniObject m_object{};

    Fn<QString()> jServerCertsPath;
    Fn<QString()> jClientPath;
    Fn<QString()> jClientCertName;
    Fn<QString()> jClientKeyName;

    Fn<FilesModel::MapType()> jServerCerts;
    Fn<FilesModel::MapType()> jClients;

    Fn<bool(const QString &)> jDeleteServerCert;
    Fn<bool(const QString &)> jDeleteClient;
    Fn<int(const QString &)> jAddServerCert;
    Fn<int(const QString &, const QString &)> jAddClientCert;
    Fn<int(const QString &, const QString &)> jAddClientKey;

    Fn<QByteArray(const QString &)> jClientCertData;
    Fn<QByteArray(const QString &)> jClientKeyData;

    Fn<QList<QByteArray>()> jTrustedCertsData;

    Fn<bool(const QString &, const QString &)> jUpdateClientEntry;
    Fn<QList<QString>()> jValidClientEntries;

    friend class JavaProxy<FileProvider>;
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
