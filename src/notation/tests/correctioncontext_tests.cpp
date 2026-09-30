/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>

#include "engraving/dom/masterscore.h"
#include "engraving/dom/measure.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/note.h"

#include "engraving/tests/utils/scorerw.h"
#include "notation/internal/correctioncontext.h"

using namespace mu::notation;
using namespace mu::engraving;

static Note* firstNote(MasterScore* score)
{
    for (Measure* m = score->firstMeasure(); m; m = m->nextMeasure()) {
        for (Segment* s = m->first(SegmentType::ChordRest); s; s = s->next(SegmentType::ChordRest)) {
            if (EngravingItem* e = s->element(0)) {
                if (e->isChord()) {
                    return toChord(e)->notes().front();
                }
            }
        }
    }
    return nullptr;
}

class Notation_CorrectionContext : public ::testing::Test {};

TEST_F(Notation_CorrectionContext, ExtractsFromFirstNote)
{
    MasterScore* score = ScoreRW::readScore(u"annotation_data/notes.mscx");
    ASSERT_TRUE(score);
    Note* n = firstNote(score);
    ASSERT_TRUE(n);

    const CorrectionContext ctx = extractContext(n);
    EXPECT_EQ(ctx.clef, QString("G"));       // default treble
    EXPECT_EQ(ctx.timeSig, QString("4/4"));
    EXPECT_EQ(ctx.staffIdx, 0);
    EXPECT_EQ(ctx.pitch, 60);                // C4
    EXPECT_EQ(ctx.measureIndex, 0);
    EXPECT_EQ(ctx.beatPos, QString("0/1"));  // first beat, rtick 0
    EXPECT_EQ(ctx.voice, 0);
    EXPECT_EQ(ctx.nextLabel, QString("Quarter-Note"));  // next chord/rest exists

    delete score;
}

TEST_F(Notation_CorrectionContext, NullElementIsSafe)
{
    const CorrectionContext ctx = extractContext(nullptr);
    EXPECT_EQ(ctx.pitch, -1);
    EXPECT_TRUE(ctx.clef.isEmpty());
}
