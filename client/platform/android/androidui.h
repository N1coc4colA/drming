#ifndef ANDROIDUIPLATFORM_H
#define ANDROIDUIPLATFORM_H

#include <QJniObject>
#include <QObject>

namespace Platform {

class AndroidUi : public QObject
{
public:
    explicit AndroidUi(QObject *parent = nullptr);

    Q_INVOKABLE void enterFullScreen();
    Q_INVOKABLE void leaveFullScreen();

private:
    QJniObject m_javaHelper{};
};

} // namespace Platform

#endif // ANDROIDUIPLATFORM_H
