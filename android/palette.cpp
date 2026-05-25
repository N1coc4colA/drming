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

    const auto palette = qApp->palette();

    // Save the brushes
    for (int group = static_cast<int>(QPalette::ColorGroup::Active); group < static_cast<int>(QPalette::ColorGroup::NColorGroups); group++) {
        for (int role = static_cast<int>(QPalette::ColorGroup::Active); role < static_cast<int>(QPalette::ColorRole::NColorRoles); role++) {
            if (palette.isBrushSet(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role))) {
                stream << false;
                continue;
            }

            stream << true << palette.brush(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role));
        }
    }

    // Save the colors
    for (int group = static_cast<int>(QPalette::ColorGroup::Active); group < static_cast<int>(QPalette::ColorGroup::NColorGroups); group++) {
        for (int role = static_cast<int>(QPalette::ColorGroup::Active); role < static_cast<int>(QPalette::ColorRole::NColorRoles); role++) {
            stream << palette.color(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role));
        }
    }
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

    // Load the brushes
    for (int group = static_cast<int>(QPalette::ColorGroup::Active); group < static_cast<int>(QPalette::ColorGroup::NColorGroups); group++) {
        for (int role = static_cast<int>(QPalette::ColorGroup::Active); role < static_cast<int>(QPalette::ColorRole::NColorRoles); role++) {
            bool isSet = false;
            stream >> isSet;
            if (!isSet) {
                continue;
            }

            QBrush brush{};
            stream >> brush;
            palette.setBrush(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role), brush);
        }
    }

    // Save the colors
    for (int group = static_cast<int>(QPalette::ColorGroup::Active); group < static_cast<int>(QPalette::ColorGroup::NColorGroups); group++) {
        for (int role = static_cast<int>(QPalette::ColorGroup::Active); role < static_cast<int>(QPalette::ColorRole::NColorRoles); role++) {
            QColor color{};
            stream >> color;
            palette.setColor(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role), color);
        }
    }

    return palette;
}
