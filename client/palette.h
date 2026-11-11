#ifndef PALETTE_H
#define PALETTE_H

#include <QPalette>

bool writePalette(const QString &fp);
std::optional<QPalette> readPalette(const QString &fp);

#endif // PALETTE_H
