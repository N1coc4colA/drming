#ifndef PALETTE_H
#define PALETTE_H

#include <QPalette>

/// Serializes the application palette to a file.
bool writePalette(const QString &fp);
/// Reads a serialized palette, or returns no value when unavailable.
std::optional<QPalette> readPalette(const QString &fp);

#endif // PALETTE_H
