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

    void deleteServerCert(const QString &file) override;
    void deleteClientCert(const QString &file) override;

    void addServerCert() override;
    void addClientCerts() override;

    inline bool hasError() const { return m_dirCreationError; }
    inline QString errorMessage() const { return m_errorMessage; }

    static constexpr auto folderName = "drming";

private:
    bool m_dirCreationError = false;
    QString m_errorMessage{};
};

} // namespace Platform

#endif // FILEPROVIDERPLATFORM_H
