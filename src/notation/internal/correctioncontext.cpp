/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#include "correctioncontext.h"

#include "engraving/dom/chord.h"
#include "engraving/dom/chordrest.h"
#include "engraving/dom/engravingitem.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/note.h"
#include "engraving/dom/score.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/staff.h"
#include "engraving/types/fraction.h"
#include "engraving/types/types.h"

using namespace mu::notation;
using namespace mu::engraving;

static QString clefToString(ClefType c)
{
    switch (c) {
    case ClefType::G: case ClefType::G8_VA: case ClefType::G8_VB:
    case ClefType::G15_MA: case ClefType::G15_MB: case ClefType::G8_VB_O:
    case ClefType::G8_VB_P: case ClefType::G_1:
        return "G";
    case ClefType::F: case ClefType::F8_VB: case ClefType::F15_MB:
    case ClefType::F_8VA: case ClefType::F_15MA: case ClefType::F_B:
    case ClefType::F_C: case ClefType::F_F18C: case ClefType::F_19C:
        return "F";
    case ClefType::C1: case ClefType::C2: case ClefType::C3:
    case ClefType::C4: case ClefType::C5:
        return "C";
    default:
        return "";
    }
}

static QString fracStr(const Fraction& f)
{
    return QString("%1/%2").arg(f.numerator()).arg(f.denominator());
}

// Coarse label for a neighbour ChordRest (best-effort context, not a HOMUS label).
static QString neighbourLabel(const Segment* fromSeg, track_idx_t track, bool forward)
{
    if (!fromSeg) {
        return "";
    }
    for (const Segment* s = forward ? fromSeg->next1(SegmentType::ChordRest)
                                    : fromSeg->prev1(SegmentType::ChordRest);
         s; s = forward ? s->next1(SegmentType::ChordRest) : s->prev1(SegmentType::ChordRest)) {
        if (EngravingItem* e = s->element(track)) {
            if (e->isRest()) {
                return "Rest";
            }
            if (e->isChord()) {
                switch (toChord(e)->durationType().type()) {
                case DurationType::V_WHOLE:   return "Whole-Note";
                case DurationType::V_HALF:    return "Half-Note";
                case DurationType::V_QUARTER: return "Quarter-Note";
                case DurationType::V_EIGHTH:  return "Eighth-Note";
                case DurationType::V_16TH:    return "Sixteenth-Note";
                default:                      return "";
                }
            }
        }
    }
    return "";
}

CorrectionContext mu::notation::extractContext(const EngravingItem* el)
{
    CorrectionContext ctx;
    if (!el) {
        return ctx;
    }

    const Segment* seg = nullptr;
    if (el->isNote()) {
        const Note* n = toNote(el);
        ctx.pitch = n->pitch();
        ctx.line = n->line();
        if (const Chord* ch = n->chord()) {
            seg = ch->segment();
        }
    } else if (const ChordRest* cr = el->isChordRest() ? toChordRest(el) : nullptr) {
        seg = cr->segment();
    }

    ctx.staffIdx = static_cast<int>(el->staffIdx());
    ctx.voice = static_cast<int>(el->voice());

    const Fraction tick = el->tick();
    const Score* score = el->score();
    if (score) {
        if (const Staff* st = score->staff(el->staffIdx())) {
            ctx.clef = clefToString(st->clef(tick));
            ctx.key = static_cast<int>(st->key(tick));
        }
    }

    if (seg) {
        if (const Measure* m = seg->measure()) {
            ctx.timeSig = fracStr(m->timesig());
            ctx.measureIndex = m->measureIndex();
            const Fraction rt = seg->rtick();
            ctx.beatPos = fracStr(rt);
            ctx.beatRemaining = fracStr(m->ticks() - rt);
        }
        ctx.prevLabel = neighbourLabel(seg, el->track(), /*forward*/ false);
        ctx.nextLabel = neighbourLabel(seg, el->track(), /*forward*/ true);
    }

    return ctx;
}
