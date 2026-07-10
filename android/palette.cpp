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

    // For each group, only save colors that differ from the "normal" group
    for (int group = QPalette::Active; group < QPalette::NColorGroups; group++) {
        for (int role = QPalette::Window; role < QPalette::NColorRoles; role++) {
            const QColor color = palette.color(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role));

            // Only save if different from default or if it's a brush
            const bool isBrush = palette.isBrushSet(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role));

            stream << isBrush;
            if (isBrush) {
                stream << palette.brush(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role));
            } else {
                stream << color; // Save just the color
            }
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

    for (int group = QPalette::Active; group < QPalette::NColorGroups; group++) {
        for (int role = QPalette::Window; role < QPalette::NColorRoles; role++) {
            bool isBrush;
            stream >> isBrush;

            if (isBrush) {
                QBrush brush;
                stream >> brush;
                palette.setBrush(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role), brush);
            } else {
                QColor color;
                stream >> color;
                palette.setColor(static_cast<QPalette::ColorGroup>(group), static_cast<QPalette::ColorRole>(role), color);
            }
        }
    }

    return palette;
}
