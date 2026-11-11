#include "palette.h"

#include <QDataStream>
#include <QFile>
#include <QGuiApplication>

bool writePalette(const QString &fp)
{
    QFile save(fp);
    if (!save.open(QIODeviceBase::WriteOnly)) {
        return false;
    }

    QDataStream stream(&save);
    stream.setByteOrder(QDataStream::BigEndian);

    stream << qApp->palette();

    return true;
}

std::optional<QPalette> readPalette(const QString &fp)
{
    QFile save(fp);
    if (!save.open(QIODeviceBase::ReadOnly)) {
        return {};
    }

    QPalette palette{};
    QDataStream stream(&save);
    stream.setByteOrder(QDataStream::BigEndian);

    stream >> palette;
    return palette;
}
