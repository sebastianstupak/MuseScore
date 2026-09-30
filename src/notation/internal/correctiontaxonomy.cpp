/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#include "correctiontaxonomy.h"

#include <QJsonObject>

using namespace mu::notation;

const QList<TaxonEntry>& mu::notation::taxonomy()
{
    static const QList<TaxonEntry> k = {
        // Notes (in model vocab; applied via note input) — id, display, group, kind, applyKind, applyParam, homusLabel
        { "note.whole",     "Whole note",     "Notes", "point", "note-input", "whole",   "Whole-Note" },
        { "note.half",      "Half note",      "Notes", "point", "note-input", "half",    "Half-Note" },
        { "note.quarter",   "Quarter note",   "Notes", "point", "note-input", "quarter", "Quarter-Note" },
        { "note.eighth",    "Eighth note",    "Notes", "point", "note-input", "eighth",  "Eighth-Note" },
        { "note.16th",      "16th note",      "Notes", "point", "note-input", "16th",    "Sixteenth-Note" },
        { "note.32nd",      "32nd note",      "Notes", "point", "note-input", "32nd",    "Thirty-Two-Note" },
        { "note.64th",      "64th note",      "Notes", "point", "note-input", "64th",    "Sixty-Four-Note" },
        // Rests (in model vocab; applied via note input)
        { "rest.whole_half", "Whole/half rest", "Rests", "point", "note-input", "whole",   "Whole-Half-Rest" },
        { "rest.quarter",    "Quarter rest",    "Rests", "point", "note-input", "quarter", "Quarter-Rest" },
        { "rest.eighth",     "Eighth rest",     "Rests", "point", "note-input", "eighth",  "Eighth-Rest" },
        { "rest.16th",       "16th rest",       "Rests", "point", "note-input", "16th",    "Sixteenth-Rest" },
        { "rest.32nd",       "32nd rest",       "Rests", "point", "note-input", "32nd",    "Thirty-Two-Rest" },
        { "rest.64th",       "64th rest",       "Rests", "point", "note-input", "64th",    "Sixty-Four-Rest" },
        // Accidentals (in model vocab; palette drop)
        { "accidental.sharp",        "Sharp",        "Accidentals", "point", "palette-drop", "sharp",        "Sharp" },
        { "accidental.flat",         "Flat",         "Accidentals", "point", "palette-drop", "flat",         "Flat" },
        { "accidental.natural",      "Natural",      "Accidentals", "point", "palette-drop", "natural",      "Natural" },
        { "accidental.double_sharp", "Double sharp", "Accidentals", "point", "palette-drop", "double_sharp", "Double-Sharp" },
        // Clefs (in model vocab; palette drop)
        { "clef.g", "Treble clef", "Clefs", "point", "palette-drop", "g_clef", "G-Clef" },
        { "clef.f", "Bass clef",   "Clefs", "point", "palette-drop", "f_clef", "F-Clef" },
        { "clef.c", "Alto clef",   "Clefs", "point", "palette-drop", "c_clef", "C-Clef" },
        // Capture-only markings (NOT yet in model vocab; the flywheel collects their ink) —
        // point/attached applied via palette drop later; spanners captured-only (applyKind "none").
        { "dynamic.f",  "Forte",    "Dynamics",      "attached", "palette-drop", "f",  "" },
        { "dynamic.p",  "Piano",    "Dynamics",      "attached", "palette-drop", "p",  "" },
        { "articulation.staccato", "Staccato", "Articulations", "attached", "palette-drop", "staccato", "" },
        { "articulation.legato",   "Legato",   "Articulations", "attached", "palette-drop", "legato",   "" },
        { "line.pedal",   "Pedal",   "Lines", "spanner", "none", "pedal",   "" },
        { "line.hairpin", "Hairpin", "Lines", "spanner", "none", "hairpin", "" },
    };
    return k;
}

QJsonObject mu::notation::taxonLabelObject(const TaxonEntry& e)
{
    QJsonObject o;
    o.insert("id", e.id);
    o.insert("display", e.display);
    o.insert("group", e.group);
    o.insert("kind", e.kind);
    if (!e.homusLabel.isEmpty()) {
        o.insert("model_label", e.homusLabel);   // A1 `neume export` reads this for HOMUS .txt
    }
    return o;
}

const TaxonEntry* mu::notation::taxonByHomusLabel(const QString& homusLabel)
{
    if (homusLabel.isEmpty()) {
        return nullptr;
    }
    for (const TaxonEntry& e : taxonomy()) {
        if (e.homusLabel == homusLabel) {
            return &e;
        }
    }
    return nullptr;
}
