/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>
#include <QJsonObject>
#include "notation/internal/correctiontaxonomy.h"

using namespace mu::notation;

TEST(Notation_CorrectionTaxonomy, CoversRecognizerLabelsAndMore)
{
    const QList<TaxonEntry>& t = taxonomy();
    // The 7 note durations + 4 accidentals + 3 clefs the recognizer emits are present,
    // plus at least one capture-only marking group (Dynamics/Articulations).
    EXPECT_GE(t.size(), 14);
    bool hasDynamics = false;
    for (const TaxonEntry& e : t) {
        if (e.group == "Dynamics") { hasDynamics = true; }
    }
    EXPECT_TRUE(hasDynamics);
}

TEST(Notation_CorrectionTaxonomy, HomusReverseLookupAndLabelObject)
{
    const TaxonEntry* e = taxonByHomusLabel("Eighth-Note");
    ASSERT_NE(e, nullptr);
    EXPECT_EQ(e->id, QString("note.eighth"));
    EXPECT_EQ(e->applyKind, QString("note-input"));
    EXPECT_EQ(e->applyParam, QString("eighth"));

    const QJsonObject o = taxonLabelObject(*e);
    EXPECT_EQ(o.value("id").toString(), QString("note.eighth"));
    EXPECT_EQ(o.value("kind").toString(), QString("point"));
    EXPECT_EQ(o.value("model_label").toString(), QString("Eighth-Note"));  // A1 export reads this
}

TEST(Notation_CorrectionTaxonomy, CaptureOnlyMarkingHasNoHomusLabel)
{
    // A dynamics entry has no HOMUS label (not in model vocab yet).
    const TaxonEntry* dyn = nullptr;
    for (const TaxonEntry& x : taxonomy()) {
        if (x.group == "Dynamics") { dyn = &x; break; }
    }
    ASSERT_NE(dyn, nullptr);
    EXPECT_TRUE(dyn->homusLabel.isEmpty());
    // Observable behavior: taxonLabelObject omits model_label for capture-only entries.
    const QJsonObject o = taxonLabelObject(*dyn);
    EXPECT_FALSE(o.contains("model_label"));
    EXPECT_EQ(o.value("kind").toString(), QString("attached"));
}
