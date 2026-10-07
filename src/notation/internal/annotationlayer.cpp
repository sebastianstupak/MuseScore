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

#include "annotationlayer.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>

#include "engraving/infrastructure/mscreader.h"
#include "engraving/infrastructure/mscwriter.h"

#include "engraving/dom/score.h"
#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/system.h"
#include "engraving/style/style.h"

// NOTE (4.7 port): engraving/editing/transaction/{transaction,undoablecommand}.h
// do not exist on 4.7 -- the Transaction/UndoableCommand framework is new in 5.0.
// See endUndoableEdit() below for what that costs here.

#include "notation/internal/inotationundostack.h"

#include "global/types/translatablestring.h"

#include "engraving/infrastructure/eid.h"
#include "engraving/infrastructure/eidregister.h"

using namespace mu;
using namespace mu::notation;
using namespace muse;
using namespace muse::draw;

// InkEditCommand (a mu::engraving::UndoableCommand that swaps the strokes vector
// between its before and after states) is omitted on the 4.7 branch: its base
// class ships only in 5.0. See endUndoableEdit().

void AnnotationLayer::setScore(engraving::Score* score)
{
    if (m_score == score) {
        return;
    }
    m_score = score;
    m_anchorsDirty = true;
}

double AnnotationLayer::spatium() const
{
    return m_score ? m_score->style().spatium() : 0.0;
}

std::vector<muse::PointF> AnnotationLayer::takeCurrentStroke()
{
    std::vector<muse::PointF> pts = std::move(m_current.points);
    m_current = Stroke();
    m_drawing = false;
    return pts;
}

void AnnotationLayer::beginStroke(const muse::PointF& logicalPos)
{
    m_current = Stroke();
    m_current.color = m_color;
    m_current.width = m_width;
    m_current.points.push_back(logicalPos);
    m_drawing = true;
}

void AnnotationLayer::appendPoint(const muse::PointF& logicalPos)
{
    if (m_drawing) {
        m_current.points.push_back(logicalPos);
    }
}

void AnnotationLayer::endStroke()
{
    bool committed = false;
    if (m_drawing && m_current.points.size() > 1) {
        computeAnchor(m_current);   // pin the stroke to the measure it was drawn over
        m_strokes.push_back(m_current);
        committed = true;
    }
    m_current = Stroke();
    m_drawing = false;

    if (committed) {
        ++m_editSeq;
        m_changed.notify();
    }
}

void AnnotationLayer::eraseAt(const muse::PointF& logicalPos, double radius)
{
    recomputeAnchorsIfNeeded();

    const double r2 = radius * radius;
    bool removed = false;
    for (auto it = m_strokes.begin(); it != m_strokes.end();) {
        const double tx = it->xlate.x();
        const double ty = it->xlate.y();
        bool hit = false;
        for (const muse::PointF& p : it->points) {
            const double dx = (p.x() + tx) - logicalPos.x();
            const double dy = (p.y() + ty) - logicalPos.y();
            if (dx * dx + dy * dy <= r2) {
                hit = true;
                break;
            }
        }
        if (hit) {
            it = m_strokes.erase(it);
            removed = true;
        } else {
            ++it;
        }
    }
    if (removed) {
        ++m_editSeq;
        m_changed.notify();
    }
}

void AnnotationLayer::clear()
{
    const bool hadContent = !m_strokes.empty();
    m_strokes.clear();
    m_current = Stroke();
    m_drawing = false;
    if (hadContent) {
        ++m_editSeq;
        m_changed.notify();
    }
}

void AnnotationLayer::beginUndoableEdit()
{
    m_editBefore = m_strokes;
    m_editBeforeSeq = m_editSeq;
}

void AnnotationLayer::endUndoableEdit(const INotationUndoStackPtr& undoStack)
{
    // KNOWN GAP ON 4.7: ink edits are not undoable.
    //
    // On 5.0 this pushes an InkEditCommand through
    // undoStack->transaction(..., tx.pushWithoutPerforming(...)) -- recording a
    // change that already happened in the layer, without re-performing it and
    // without touching the engraving DOM.
    //
    // 4.7 has no equivalent reachable from here. Its INotationUndoStack offers
    // prepareChanges/commitChanges, which wrap *score* mutations; ink lives
    // outside the engraving undo system, and 4.7 exposes no hook to push an
    // arbitrary UndoCommand through the notation interface. Supporting it would
    // mean extending INotationUndoStack and NotationUndoStack -- a change to
    // upstream interfaces, well beyond porting this patch.
    //
    // So: drawing and erasing ink works, and Ctrl+Z does not undo it. Everything
    // else in the annotation layer is unaffected. Revisit if ink undo matters
    // more than staying close to upstream 4.7.
    (void)undoStack;
    m_editBefore.clear();
}

void AnnotationLayer::restoreStrokes(std::vector<Stroke> strokes)
{
    m_strokes = std::move(strokes);
    m_current = Stroke();
    m_drawing = false;
    m_anchorsDirty = true;
    ++m_editSeq;
    m_changed.notify();
}

void AnnotationLayer::computeAnchor(Stroke& stroke) const
{
    stroke.anchored = false;
    stroke.xlate = muse::PointF();
    if (!m_score || stroke.points.size() < 2) {
        return;
    }

    // Anchor point = centroid of the stroke (representative of what it circles/marks).
    double cx = 0.0, cy = 0.0;
    for (const muse::PointF& p : stroke.points) {
        cx += p.x();
        cy += p.y();
    }
    const muse::PointF centroid(cx / stroke.points.size(), cy / stroke.points.size());

    // Ink logical space == engraving canvas space, so the centroid feeds pos2measure directly.
    engraving::staff_idx_t staffIdx = muse::nidx;
    engraving::Segment* seg = nullptr;
    muse::PointF off;
    engraving::Measure* m = m_score->pos2measure(centroid, &staffIdx, nullptr, &seg, &off);
    if (!m || staffIdx == muse::nidx || !m->system()) {
        return;   // not over a measure (title/margins) -> leave absolute / page-anchored
    }

    engraving::EID eid = m->eid();
    if (!eid.isValid()) {
        eid = m->assignNewEID();   // ensure a stable id exists and gets persisted
    }

    stroke.measureEid = eid.toStdString();
    stroke.staffIdx = static_cast<int>(staffIdx);
    stroke.tick = seg ? seg->tick().ticks() : m->tick().ticks();
    const double ox = m->canvasPos().x() + (seg ? seg->x() : 0.0);
    const double oy = m->system()->staffCanvasYpage(staffIdx);
    stroke.savedOrigin = muse::PointF(ox, oy);
    stroke.anchored = true;
}

void AnnotationLayer::recomputeAnchorsIfNeeded()
{
    if (!m_anchorsDirty) {
        return;
    }
    m_anchorsDirty = false;

    for (Stroke& s : m_strokes) {
        s.xlate = muse::PointF();
        if (!s.anchored || !m_score) {
            continue;
        }
        engraving::EID eid = engraving::EID::fromStdString(s.measureEid);
        if (!eid.isValid()) {
            continue;
        }
        engraving::EngravingObject* o = m_score->masterScore()->eidRegister()->itemFromEID(eid);
        if (!o || o->type() != engraving::ElementType::MEASURE) {
            continue;   // measure deleted -> keep the stroke at its last absolute position
        }
        engraving::Measure* m = static_cast<engraving::Measure*>(o);
        if (!m->system()) {
            continue;
        }
        engraving::Segment* seg = m->findSegment(engraving::SegmentType::ChordRest, engraving::Fraction::fromTicks(s.tick));
        const double ox = m->canvasPos().x() + (seg ? seg->x() : 0.0);
        const double oy = m->system()->staffCanvasYpage(static_cast<engraving::staff_idx_t>(s.staffIdx));
        s.xlate = muse::PointF(ox - s.savedOrigin.x(), oy - s.savedOrigin.y());
    }
}

void AnnotationLayer::drawStroke(muse::draw::Painter* painter, const Stroke& stroke) const
{
    if (stroke.points.size() < 2) {
        return;
    }
    painter->setPen(Pen(stroke.color, stroke.width, PenStyle::SolidLine, PenCapStyle::RoundCap, PenJoinStyle::RoundJoin));
    const double tx = stroke.xlate.x();
    const double ty = stroke.xlate.y();
    for (size_t i = 1; i < stroke.points.size(); ++i) {
        const muse::PointF& a = stroke.points[i - 1];
        const muse::PointF& b = stroke.points[i];
        painter->drawLine(LineF(a.x() + tx, a.y() + ty, b.x() + tx, b.y() + ty));
    }
}

void AnnotationLayer::paint(muse::draw::Painter* painter)
{
    recomputeAnchorsIfNeeded();
    for (const Stroke& stroke : m_strokes) {
        drawStroke(painter, stroke);
    }
    if (m_drawing) {
        drawStroke(painter, m_current);
    }
}

muse::Ret AnnotationLayer::write(engraving::MscWriter& writer, const muse::io::path_t& pathPrefix) const
{
    QJsonArray strokesArr;
    for (const Stroke& s : m_strokes) {
        QJsonArray pts;
        for (const muse::PointF& p : s.points) {
            pts.append(p.x());
            pts.append(p.y());
        }

        QJsonObject so;
        so["r"] = s.color.red();
        so["g"] = s.color.green();
        so["b"] = s.color.blue();
        so["a"] = s.color.alpha();
        so["width"] = s.width;
        so["points"] = pts;

        if (s.anchored) {
            QJsonObject anchor;
            anchor["m"] = QString::fromStdString(s.measureEid);
            anchor["s"] = s.staffIdx;
            anchor["t"] = s.tick;
            anchor["ox"] = s.savedOrigin.x();
            anchor["oy"] = s.savedOrigin.y();
            so["anchor"] = anchor;
        }

        strokesArr.append(so);
    }

    QJsonObject rootObj;
    rootObj["version"] = 2;
    rootObj["strokes"] = strokesArr;

    QByteArray json = QJsonDocument(rootObj).toJson(QJsonDocument::Compact);
    writer.writeAnnotationsJsonFile(ByteArray::fromQByteArrayNoCopy(json), pathPrefix);

    return make_ret(Ret::Code::Ok);
}

muse::Ret AnnotationLayer::read(const engraving::MscReader& reader, const muse::io::path_t& pathPrefix)
{
    ByteArray json = reader.readAnnotationsJsonFile(pathPrefix);
    if (json.empty()) {
        return make_ret(Ret::Code::Ok);
    }

    m_strokes.clear();

    QJsonObject rootObj = QJsonDocument::fromJson(json.toQByteArrayNoCopy()).object();
    QJsonArray strokesArr = rootObj.value("strokes").toArray();
    for (const QJsonValue& v : strokesArr) {
        QJsonObject so = v.toObject();

        Stroke s;
        s.width = so.value("width").toDouble(15.0);
        s.color = Color(so.value("r").toInt(224), so.value("g").toInt(48), so.value("b").toInt(48), so.value("a").toInt(255));

        QJsonArray pts = so.value("points").toArray();
        for (int i = 0; i + 1 < pts.size(); i += 2) {
            s.points.push_back(muse::PointF(pts.at(i).toDouble(), pts.at(i + 1).toDouble()));
        }

        // v2: optional per-stroke reflow anchor. v1 files have no "anchor" -> stay absolute.
        if (so.contains("anchor")) {
            QJsonObject a = so.value("anchor").toObject();
            s.anchored = true;
            s.measureEid = a.value("m").toString().toStdString();
            s.staffIdx = a.value("s").toInt(0);
            s.tick = a.value("t").toInt(0);
            s.savedOrigin = muse::PointF(a.value("ox").toDouble(), a.value("oy").toDouble());
        }

        if (s.points.size() > 1) {
            m_strokes.push_back(s);
        }
    }

    m_anchorsDirty = true;
    return make_ret(Ret::Code::Ok);
}

void AnnotationLayer::makeDefault()
{
    m_strokes.clear();
    m_current = Stroke();
    m_drawing = false;
    m_anchorsDirty = true;
}
