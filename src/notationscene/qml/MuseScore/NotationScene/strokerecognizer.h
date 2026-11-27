/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore Limited and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */
#ifndef MU_NOTATION_STROKERECOGNIZER_H
#define MU_NOTATION_STROKERECOGNIZER_H

#include <vector>

#include <QByteArray>

#include "draw/types/geometry.h"

#include "notation/inotation.h"

namespace mu::notation {
// Bridges "write mode" pen gestures to the neume recognizer (run as a subprocess)
// and applies the returned intents to the score through the notation API. The
// neume binary is located via the STYLUS_NEUME_BIN environment variable.
class StrokeRecognizer
{
public:
    // Runs neume on one gesture (a symbol may be several strokes; each stroke is a
    // list of points in score/canvas coords) and applies the returned intents.
    // Returns the number of intents applied (0 if nothing was recognized or neume
    // is unavailable). `spatium` sets the recognition scale. When `additive`, a
    // lasso ADDs to the current selection instead of replacing it. Also auto-logs
    // each classified, applied symbol as an "accepted" correction sample (the
    // flywheel's unverified-positive path) via `neume record`.
    //
    // `region` says the strokes are a PHRASE of several symbols rather than one
    // gesture, which is what batch mode accumulates. It is not an optimisation:
    // without it the recognizer is asked to name the whole phrase as a single
    // shape, and three noteheads in a row are a wide zig-zag, which means erase.
    // See 64-batch-recognize-e2e and runbook 10 in the boox repo.
    // `notesOnly` is the FAST PATH, and it exists because the settle timer
    // is nearly all of handwriting's latency. A symbol can be several
    // strokes -- that is why the timer exists -- but a NOTEHEAD is one
    // stroke, and neume answers in 2-8 ms. So the editor can ask straight
    // away, place the note if that is what it is, and only wait out the
    // settle for answers that might still be the beginning of something.
    //
    // With notesOnly set, nothing is applied unless the best applicable
    // intent is a put_note, and the return is -1 for "not now, keep the
    // strokes and wait" as distinct from 0 for "nothing applied".
    int recognizeAndApply(const std::vector<std::vector<muse::PointF> >& strokes, const INotationPtr& notation,
                          double spatium, bool additive = false, bool region = false,
                          bool notesOnly = false);

private:
    QByteArray runNeume(const QByteArray& inputJson) const;

    // Send a correction record (schema-1 JSON) to `neume record`. FIRE AND
    // FORGET -- returns true once the process has been asked to start.
    // Nobody waits for the answer, and this used to block the GUI thread.
    bool recordCorrection(const QByteArray& recordJson) const;
};
}

#endif // MU_NOTATION_STROKERECOGNIZER_H
