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
#ifndef MU_NOTATION_ANNOTATIONLAYER_H
#define MU_NOTATION_ANNOTATIONLAYER_H

#include <cstdint>
#include <vector>

#include "draw/painter.h"

#include "async/notification.h"
#include "io/path.h"
#include "types/ret.h"

#include "inotationundostack.h"

namespace mu::engraving {
class MscReader;
class MscWriter;
class Score;
}

namespace mu::notation {
class InkEditCommand;

// A freehand ink layer drawn over the score. Strokes are stored in score-logical
// coordinates and painted with the notation view's world transform, so ink follows
// scroll, zoom and view-mode automatically without any coordinate bookkeeping.
//
// The layer is owned by Notation (like viewState/soloMuteState) so that it can be
// serialised into the .mscz as `annotations.json` and survive save, reload,
// autosave and crash recovery. The paint view edits and renders it, but does not
// own it.
class AnnotationLayer
{
public:
    // Drawing (positions in score-logical coordinates).
    void beginStroke(const muse::PointF& logicalPos);
    void appendPoint(const muse::PointF& logicalPos);
    void endStroke();
    bool isDrawing() const { return m_drawing; }

    // Editing.
    void eraseAt(const muse::PointF& logicalPos, double radius);
    void clear();

    // Unified undo/redo: bracket an interactive ink edit with begin/endUndoableEdit
    // so it lands on the score's undo stack (Ctrl+Z / Ctrl+Y). beginUndoableEdit
    // snapshots the strokes before the edit; endUndoableEdit pushes a command if
    // anything changed. Not used for write-mode gestures (those are transient).
    void beginUndoableEdit();
    void endUndoableEdit(const INotationUndoStackPtr& undoStack);

    // Current pen (applied to the next stroke).
    void setColor(const muse::draw::Color& color) { m_color = color; }
    muse::draw::Color color() const { return m_color; }
    void setWidth(double width) { m_width = width; }
    double width() const { return m_width; }

    int strokeCount() const { return static_cast<int>(m_strokes.size()); }

    void paint(muse::draw::Painter* painter);

    // Write mode: take the in-progress gesture's points without committing it as
    // ink (the caller hands them to recognition instead).
    std::vector<muse::PointF> takeCurrentStroke();

    // Reflow anchoring: the score this ink belongs to, used to anchor strokes to
    // measures and reproject them after relayout. Set by the owning Notation.
    void setScore(engraving::Score* score);
    // One staff space in stroke/canvas coordinates (0 if no score) — recognition scale.
    double spatium() const;
    // Mark cached anchor translations stale (call on any relayout / view-mode change).
    void invalidateAnchors() { m_anchorsDirty = true; }

    // Persistence: read/write `annotations.json` inside the .mscz container.
    muse::Ret read(const engraving::MscReader& reader, const muse::io::path_t& pathPrefix = "");
    muse::Ret write(engraving::MscWriter& writer, const muse::io::path_t& pathPrefix = "") const;
    void makeDefault();

    // Fired when the committed strokes change, so the project can flag unsaved
    // changes. Not fired by read()/makeDefault() (those are load-time, not edits).
    muse::async::Notification changed() const { return m_changed; }

private:
    struct Stroke {
        std::vector<muse::PointF> points;   // absolute canvas coords, valid in the layout at draw/save time
        muse::draw::Color color;
        double width = 15.0;

        // Per-stroke rigid anchor to a measure + staff + beat. When resolvable, the
        // whole stroke is translated by (current anchor origin - savedOrigin) so it
        // rides along with the notes across relayout. Unanchored strokes (title/margin
        // ink, or v1 files) stay at their absolute positions.
        bool anchored = false;
        std::string measureEid;             // stable measure id (survives edits + reload)
        int staffIdx = 0;
        int tick = 0;                       // absolute tick of the anchor segment
        muse::PointF savedOrigin;           // anchor origin in canvas coords at draw/save time

        muse::PointF xlate;                 // runtime cache: current translation (not serialised)
    };

    void drawStroke(muse::draw::Painter* painter, const Stroke& stroke) const;
    void computeAnchor(Stroke& stroke) const;   // resolve anchor at draw time
    void recomputeAnchorsIfNeeded();            // refresh cached translations after relayout
    void restoreStrokes(std::vector<Stroke> strokes);   // used by the undo command

    friend class InkEditCommand;

    std::vector<Stroke> m_strokes;   // committed strokes
    Stroke m_current;                // stroke in progress
    bool m_drawing = false;

    std::vector<Stroke> m_editBefore;   // strokes snapshot at beginUndoableEdit()
    uint64_t m_editSeq = 0;             // bumped on every committed change
    uint64_t m_editBeforeSeq = 0;       // m_editSeq captured at beginUndoableEdit()

    muse::draw::Color m_color = muse::draw::Color(224, 48, 48);   // red
    double m_width = 15.0;   // logical units (~0.6 staff-space); scales with zoom

    engraving::Score* m_score = nullptr;   // for anchoring/reprojection; not owned
    bool m_anchorsDirty = true;            // cached translations need refresh

    muse::async::Notification m_changed;
};
}

#endif // MU_NOTATION_ANNOTATIONLAYER_H
