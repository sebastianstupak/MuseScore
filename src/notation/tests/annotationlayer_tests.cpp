/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2025 MuseScore Limited and others
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

#include <gtest/gtest.h>

#include "io/buffer.h"

#include "async/channel.h"
#include "async/notification.h"

#include "draw/types/geometry.h"
#include "draw/types/color.h"

#include "engraving/dom/masterscore.h"
#include "engraving/infrastructure/mscreader.h"
#include "engraving/infrastructure/mscwriter.h"

#include "engraving/tests/utils/scorerw.h"

#include "notation/internal/annotationlayer.h"
#include "notation/internal/notationundostack.h"
#include "notation/internal/igetscore.h"

using namespace mu::notation;
using namespace mu::engraving;
using namespace muse;
using namespace muse::io;
using namespace muse::draw;

static const String ANNOTATION_DATA_DIR(u"annotation_data/");

//! A trivial IGetScore for driving a real NotationUndoStack over a test score.
class TestGetScore : public IGetScore
{
public:
    void setScore(mu::engraving::Score* score) { m_score = score; }
    mu::engraving::Score* score() const override { return m_score; }
    muse::async::Notification scoreInited() const override { return m_inited; }

private:
    mu::engraving::Score* m_score = nullptr;
    muse::async::Notification m_inited;
};

class Notation_AnnotationLayerTests : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_score = ScoreRW::readScore(ANNOTATION_DATA_DIR + u"simple.mscx");
        ASSERT_TRUE(m_score);
        m_getScore.setScore(m_score);
    }

    void TearDown() override
    {
        delete m_score;
        m_score = nullptr;
    }

    //! One committed 3-point stroke.
    static void drawStroke(AnnotationLayer& layer, const PointF& a, const PointF& b, const PointF& c)
    {
        layer.beginStroke(a);
        layer.appendPoint(b);
        layer.appendPoint(c);
        layer.endStroke();
    }

    //! A committed stroke wrapped in an undoable edit, pushed onto `undoStack`.
    static void undoableDraw(AnnotationLayer& layer, const INotationUndoStackPtr& undoStack,
                             const PointF& a, const PointF& b, const PointF& c)
    {
        layer.beginUndoableEdit();
        drawStroke(layer, a, b, c);
        layer.endUndoableEdit(undoStack);
    }

    INotationUndoStackPtr makeUndoStack()
    {
        return std::make_shared<NotationUndoStack>(&m_getScore, muse::async::Channel<muse::RectF>());
    }

    MasterScore* m_score = nullptr;
    TestGetScore m_getScore;
};

// ---------------------------------------------------------------------------
// Stroke semantics
// ---------------------------------------------------------------------------

TEST_F(Notation_AnnotationLayerTests, DrawCommitsStroke)
{
    AnnotationLayer layer;
    layer.setScore(m_score);

    EXPECT_EQ(layer.strokeCount(), 0);
    drawStroke(layer, PointF(10, 10), PointF(20, 20), PointF(30, 15));
    EXPECT_EQ(layer.strokeCount(), 1);
}

TEST_F(Notation_AnnotationLayerTests, ShortStrokeIsRejected)
{
    AnnotationLayer layer;
    layer.setScore(m_score);

    // A single point is a tap, not a stroke: nothing should be committed.
    layer.beginStroke(PointF(10, 10));
    layer.endStroke();
    EXPECT_EQ(layer.strokeCount(), 0);
}

TEST_F(Notation_AnnotationLayerTests, EraseRemovesNearbyStroke)
{
    AnnotationLayer layer;
    layer.setScore(m_score);

    drawStroke(layer, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    drawStroke(layer, PointF(400, 300), PointF(410, 300), PointF(420, 300));
    ASSERT_EQ(layer.strokeCount(), 2);

    layer.eraseAt(PointF(110, 100), 20.0);
    EXPECT_EQ(layer.strokeCount(), 1);
}

TEST_F(Notation_AnnotationLayerTests, EraseMissLeavesStrokes)
{
    AnnotationLayer layer;
    layer.setScore(m_score);

    drawStroke(layer, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    ASSERT_EQ(layer.strokeCount(), 1);

    layer.eraseAt(PointF(900, 900), 20.0);   // far away
    EXPECT_EQ(layer.strokeCount(), 1);
}

TEST_F(Notation_AnnotationLayerTests, ClearRemovesAll)
{
    AnnotationLayer layer;
    layer.setScore(m_score);

    drawStroke(layer, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    drawStroke(layer, PointF(400, 300), PointF(410, 300), PointF(420, 300));
    ASSERT_EQ(layer.strokeCount(), 2);

    layer.clear();
    EXPECT_EQ(layer.strokeCount(), 0);
}

// ---------------------------------------------------------------------------
// Persistence (annotations.json inside the .mscz container)
// ---------------------------------------------------------------------------

TEST_F(Notation_AnnotationLayerTests, WriteReadRoundTrip)
{
    AnnotationLayer src;
    src.setScore(m_score);
    src.setColor(Color(10, 20, 30));
    src.setWidth(7.0);
    drawStroke(src, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    drawStroke(src, PointF(200, 150), PointF(210, 150), PointF(220, 150));
    drawStroke(src, PointF(300, 250), PointF(310, 250), PointF(320, 250));
    ASSERT_EQ(src.strokeCount(), 3);

    ByteArray msczData;
    {
        Buffer buf(&msczData);
        MscWriter::Params params;
        params.device = &buf;
        params.filePath = u"ink.mscz";
        params.mode = MscIoMode::Zip;

        MscWriter writer(params);
        writer.open();
        src.write(writer);
    }

    AnnotationLayer dst;
    dst.setScore(m_score);
    {
        Buffer buf(&msczData);
        MscReader::Params params;
        params.device = &buf;
        params.filePath = u"ink.mscz";
        params.mode = MscIoMode::Zip;

        MscReader reader(params);
        reader.open();
        dst.read(reader);
    }

    EXPECT_EQ(dst.strokeCount(), 3);

    // The point data survived: erasing where a stroke was drawn removes it.
    dst.eraseAt(PointF(110, 100), 20.0);
    EXPECT_EQ(dst.strokeCount(), 2);
}

TEST_F(Notation_AnnotationLayerTests, WriteReadEmptyLayer)
{
    AnnotationLayer src;
    src.setScore(m_score);
    ASSERT_EQ(src.strokeCount(), 0);

    ByteArray msczData;
    {
        Buffer buf(&msczData);
        MscWriter::Params params;
        params.device = &buf;
        params.filePath = u"ink.mscz";
        params.mode = MscIoMode::Zip;

        MscWriter writer(params);
        writer.open();
        src.write(writer);
    }

    AnnotationLayer dst;
    dst.setScore(m_score);
    {
        Buffer buf(&msczData);
        MscReader::Params params;
        params.device = &buf;
        params.filePath = u"ink.mscz";
        params.mode = MscIoMode::Zip;

        MscReader reader(params);
        reader.open();
        dst.read(reader);
    }

    EXPECT_EQ(dst.strokeCount(), 0);
}

// ---------------------------------------------------------------------------
// Unified undo / redo through the score's undo stack
// ---------------------------------------------------------------------------

TEST_F(Notation_AnnotationLayerTests, UndoRedoDrawnStroke)
{
    INotationUndoStackPtr undoStack = makeUndoStack();

    AnnotationLayer layer;
    layer.setScore(m_score);

    undoableDraw(layer, undoStack, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    EXPECT_EQ(layer.strokeCount(), 1);
    EXPECT_TRUE(undoStack->canUndo());

    undoStack->undo(nullptr);
    EXPECT_EQ(layer.strokeCount(), 0);
    EXPECT_TRUE(undoStack->canRedo());

    undoStack->redo(nullptr);
    EXPECT_EQ(layer.strokeCount(), 1);
}

TEST_F(Notation_AnnotationLayerTests, UndoRedoErase)
{
    INotationUndoStackPtr undoStack = makeUndoStack();

    AnnotationLayer layer;
    layer.setScore(m_score);

    undoableDraw(layer, undoStack, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    undoableDraw(layer, undoStack, PointF(400, 300), PointF(410, 300), PointF(420, 300));
    ASSERT_EQ(layer.strokeCount(), 2);

    // Erase one stroke as a single undoable edit.
    layer.beginUndoableEdit();
    layer.eraseAt(PointF(110, 100), 20.0);
    layer.endUndoableEdit(undoStack);
    ASSERT_EQ(layer.strokeCount(), 1);

    undoStack->undo(nullptr);
    EXPECT_EQ(layer.strokeCount(), 2);   // erased stroke comes back

    undoStack->redo(nullptr);
    EXPECT_EQ(layer.strokeCount(), 1);
}

TEST_F(Notation_AnnotationLayerTests, UndoRedoClear)
{
    INotationUndoStackPtr undoStack = makeUndoStack();

    AnnotationLayer layer;
    layer.setScore(m_score);

    undoableDraw(layer, undoStack, PointF(100, 100), PointF(110, 100), PointF(120, 100));
    undoableDraw(layer, undoStack, PointF(400, 300), PointF(410, 300), PointF(420, 300));
    ASSERT_EQ(layer.strokeCount(), 2);

    layer.beginUndoableEdit();
    layer.clear();
    layer.endUndoableEdit(undoStack);
    ASSERT_EQ(layer.strokeCount(), 0);

    undoStack->undo(nullptr);
    EXPECT_EQ(layer.strokeCount(), 2);   // whole clear is one undo step
}

TEST_F(Notation_AnnotationLayerTests, EmptyBracketPushesNoCommand)
{
    INotationUndoStackPtr undoStack = makeUndoStack();

    AnnotationLayer layer;
    layer.setScore(m_score);

    // A bracket around a no-op (e.g. an aborted gesture) must not create an
    // undo step, otherwise Ctrl+Z would appear to "do nothing" once per touch.
    layer.beginUndoableEdit();
    layer.endUndoableEdit(undoStack);
    EXPECT_FALSE(undoStack->canUndo());

    // A tap (single point, never committed) is likewise a no-op.
    layer.beginUndoableEdit();
    layer.beginStroke(PointF(10, 10));
    layer.endStroke();
    layer.endUndoableEdit(undoStack);
    EXPECT_FALSE(undoStack->canUndo());
    EXPECT_EQ(layer.strokeCount(), 0);
}

TEST_F(Notation_AnnotationLayerTests, MultipleEditsUndoRedoInOrder)
{
    INotationUndoStackPtr undoStack = makeUndoStack();

    AnnotationLayer layer;
    layer.setScore(m_score);

    undoableDraw(layer, undoStack, PointF(100, 100), PointF(110, 100), PointF(120, 100));   // A
    undoableDraw(layer, undoStack, PointF(400, 300), PointF(410, 300), PointF(420, 300));   // B
    ASSERT_EQ(layer.strokeCount(), 2);

    undoStack->undo(nullptr);   // undo B
    EXPECT_EQ(layer.strokeCount(), 1);

    undoStack->undo(nullptr);   // undo A
    EXPECT_EQ(layer.strokeCount(), 0);

    undoStack->redo(nullptr);   // redo A
    EXPECT_EQ(layer.strokeCount(), 1);

    undoStack->redo(nullptr);   // redo B
    EXPECT_EQ(layer.strokeCount(), 2);
}
