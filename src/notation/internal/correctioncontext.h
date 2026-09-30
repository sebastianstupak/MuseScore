/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#ifndef MU_NOTATION_CORRECTIONCONTEXT_H
#define MU_NOTATION_CORRECTIONCONTEXT_H

#include <QString>

namespace mu::engraving {
class EngravingItem;
}

namespace mu::notation {
struct CorrectionContext {
    QString clef;
    int key = 0;
    QString timeSig;
    int staffIdx = 0;
    int line = -1000;
    int pitch = -1;
    int measureIndex = 0;
    QString beatPos;
    QString beatRemaining;
    int voice = 0;
    QString prevLabel;
    QString nextLabel;
};

CorrectionContext extractContext(const mu::engraving::EngravingItem* el);
}

#endif // MU_NOTATION_CORRECTIONCONTEXT_H
