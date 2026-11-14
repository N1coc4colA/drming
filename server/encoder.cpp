#include "encoder.h"

#include <QDebug>

#include "../registrar.h"

Encoder *Encoder::createEncoder(const PacketCallback callback, const int fps)
{
    // [TODO]: This methodology has an issue, being that one lib may provide a valid
    // encoder, but the init may fail. In that case, we should try to use another lib.
    // This requires API and RT behavioour changes.

    const auto &entries = Registrar<Encoder::Entry>::instance().entries();
    qDebug() << "Creating encoder, available entries:" << entries.size();

    // Try first on encoders that do have HWA.
    for (const auto entry : entries) {
        if (entry.hasHWA()) {
            if (auto encoder = entry.getCreationFunction()(callback, fps)) {
                qDebug() << "Created encoder:" << entry.getName() << ", has HWA: yes";
                return encoder;
            }
        }
    }

    for (const auto entry : entries) {
        if (!entry.hasHWA()) {
            if (auto encoder = entry.getCreationFunction()(callback, fps)) {
                qDebug() << "Created encoder:" << entry.getName() << ", has HWA: no";
                return encoder;
            }
        }
    }

    qWarning() << "Failed to find suitable encoder backend. Currently available ones:";
    for (const auto entry : entries) {
        qWarning() << entry.getName();
    }

    return nullptr;
}
