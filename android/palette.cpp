#include "palette.h"

#include <QDataStream>
#include <QFile>
#include <QGuiApplication>

void writePalette(const QString &fp)
{
    QFile save(fp);
    if (!save.open(QIODeviceBase::WriteOnly)) {
        return;
    }

    QDataStream stream(&save);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << qApp->palette();
}

QPalette readPalette(const QString &fp)
{
    QFile save(fp);
    if (!save.open(QIODeviceBase::ReadOnly)) {
        return qApp->palette();
    }

    QPalette palette{};
    QDataStream stream(&save);
    stream.setByteOrder(QDataStream::BigEndian);

    stream >> palette;
    return palette;
}
