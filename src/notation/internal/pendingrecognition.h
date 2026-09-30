/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#ifndef MU_NOTATION_PENDINGRECOGNITION_H
#define MU_NOTATION_PENDINGRECOGNITION_H

#include <QList>
#include <QPointF>

#include "correctioncontext.h"
#include "correctionrecord.h"

namespace mu::engraving {
class EngravingItem;
}

namespace mu::notation {
// One just-recognized element still "pending" correction: the created element
// (identity key), the ink that made it, the classifier detail, and the context.
struct PendingRecognition {
    const mu::engraving::EngravingItem* element = nullptr;
    QList<QList<QPointF> > strokes;
    RecognizerResult recognized;
    CorrectionContext context;
};

// Holds the current gesture's pending recognitions until the next gesture / timeout.
// Keyed by element pointer identity; never dereferences the element.
class PendingRegistry
{
public:
    void add(const PendingRecognition& p) { m_pending.append(p); }

    const PendingRecognition* find(const mu::engraving::EngravingItem* el) const
    {
        if (!el) {
            return nullptr;
        }
        for (const PendingRecognition& p : m_pending) {
            if (p.element == el) {
                return &p;
            }
        }
        return nullptr;
    }

    void clear() { m_pending.clear(); }
    int size() const { return static_cast<int>(m_pending.size()); }

private:
    QList<PendingRecognition> m_pending;
};
}

#endif // MU_NOTATION_PENDINGRECOGNITION_H
