/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include "notation/internal/correctionrecord.h"
#include "notation/internal/correctioncontext.h"

using namespace mu::notation;

TEST(Notation_CorrectionRecord, ParsesPutNoteWithDetail)
{
    const QByteArray json = R"({"intents":[{"type":"put_note","at":{"x":1.0,"y":2.0},"duration":"eighth","confidence":0.8,"topk":[{"label":"Eighth-Note","p":0.8},{"label":"Quarter-Note","p":0.2}],"stroke_indices":[0]}]})";
    const QList<RecognizerResult> rs = parseRecognizerOutput(json);
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs[0].type, QString("put_note"));
    EXPECT_EQ(rs[0].duration, QString("eighth"));
    EXPECT_DOUBLE_EQ(rs[0].confidence, 0.8);
    ASSERT_EQ(rs[0].topk.size(), 2);
    EXPECT_EQ(rs[0].topk[0].label, QString("Eighth-Note"));
    EXPECT_EQ(rs[0].strokeIndices, QList<int>{0});
    EXPECT_DOUBLE_EQ(rs[0].at.x(), 1.0);
}

TEST(Notation_CorrectionRecord, ParsesDropAndToleratesMissingDetail)
{
    // A drop with no confidence/topk/label (older-style) still parses; defaults apply.
    const QByteArray json = R"({"intents":[{"type":"drop","element":"sharp","at":{"x":3.0,"y":4.0}}]})";
    const QList<RecognizerResult> rs = parseRecognizerOutput(json);
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs[0].type, QString("drop"));
    EXPECT_EQ(rs[0].element, QString("sharp"));
    EXPECT_TRUE(rs[0].label.isEmpty());
    EXPECT_DOUBLE_EQ(rs[0].confidence, 1.0);   // default
}

TEST(Notation_CorrectionRecord, EmptyOrBadInputYieldsEmpty)
{
    EXPECT_TRUE(parseRecognizerOutput(QByteArray()).isEmpty());
    EXPECT_TRUE(parseRecognizerOutput("not json").isEmpty());
    EXPECT_TRUE(parseRecognizerOutput(R"({"intents":[]})").isEmpty());
}

TEST(Notation_CorrectionRecord, DropsIntentWithEmptyType)
{
    // An intent object with no "type" is not a real intent and must be dropped.
    const QByteArray json = R"({"intents":[{"at":{"x":1.0,"y":2.0}},{"type":"put_note","at":{"x":3.0,"y":4.0}}]})";
    const QList<RecognizerResult> rs = parseRecognizerOutput(json);
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs[0].type, QString("put_note"));
}

TEST(Notation_CorrectionRecord, ParsesErasePath)
{
    const QByteArray json = R"({"intents":[{"type":"erase","path":[{"x":1.0,"y":2.0},{"x":3.0,"y":4.0}],"stroke_indices":[0]}]})";
    const QList<RecognizerResult> rs = parseRecognizerOutput(json);
    ASSERT_EQ(rs.size(), 1);
    EXPECT_EQ(rs[0].type, QString("erase"));
    ASSERT_EQ(rs[0].path.size(), 2);
    EXPECT_DOUBLE_EQ(rs[0].path[1].y(), 4.0);
}

TEST(Notation_CorrectionRecord, BuildsSchema1RecordWithSafeId)
{
    RecognizerResult rec;
    rec.type = "put_note";
    rec.label = "Quarter-Note";
    rec.confidence = 0.5;
    rec.topk = { { "Quarter-Note", 0.5 }, { "Eighth-Note", 0.3 } };

    CorrectionContext ctx;
    ctx.clef = "G"; ctx.timeSig = "4/4"; ctx.pitch = 64; ctx.staffIdx = 0; ctx.measureIndex = 2;

    QJsonObject label;
    label.insert("id", "note.eighth");
    label.insert("kind", "point");
    label.insert("model_label", "Eighth-Note");

    RecordMeta meta;
    meta.createdBucket = "2026-09";
    meta.engineVersion = "neume 0.0.0";
    meta.appVersion = "musescore-fork";
    meta.modelVersion = "m1";
    meta.unit = 12.0;
    // id left empty -> builder mints a safe one

    const QByteArray json = buildCorrectionRecord(
        { { QPointF(1, 2), QPointF(3, 4) } }, rec, ctx, "corrected", label, true, meta);

    const QJsonObject o = QJsonDocument::fromJson(json).object();
    EXPECT_EQ(o.value("schema").toInt(), 1);
    EXPECT_EQ(o.value("verdict").toString(), QString("corrected"));
    EXPECT_EQ(o.value("in_model_vocab").toBool(), true);
    EXPECT_EQ(o.value("created_bucket").toString(), QString("2026-09"));
    EXPECT_DOUBLE_EQ(o.value("unit").toDouble(), 12.0);
    EXPECT_EQ(o.value("model_version").toString(), QString("m1"));
    // id is safe [A-Za-z0-9_-] and non-empty (neume record rejects otherwise)
    const QString id = o.value("id").toString();
    EXPECT_TRUE(QRegularExpression("^[A-Za-z0-9_-]+$").match(id).hasMatch());
    // recognized.label is the HOMUS string; top-level label is the taxonomy object
    EXPECT_EQ(o.value("recognized").toObject().value("label").toString(), QString("Quarter-Note"));
    EXPECT_EQ(o.value("label").toObject().value("id").toString(), QString("note.eighth"));
    // strokes round-trip
    ASSERT_EQ(o.value("strokes").toArray().size(), 1);
    EXPECT_EQ(o.value("strokes").toArray().at(0).toArray().size(), 2);
    // context present
    EXPECT_EQ(o.value("context").toObject().value("clef").toString(), QString("G"));
    EXPECT_EQ(o.value("context").toObject().value("pitch").toInt(), 64);
}

TEST(Notation_CorrectionRecord, CreatedBucketFallsBackToCurrentMonth)
{
    RecognizerResult rec; rec.type = "put_note"; rec.label = "Quarter-Note";
    CorrectionContext ctx;
    QJsonObject label; label.insert("id", "note.quarter");
    RecordMeta meta; // id + createdBucket left empty -> both minted
    const QByteArray json = buildCorrectionRecord({ { QPointF(0,0), QPointF(1,1) } }, rec, ctx, "confirmed", label, true, meta);
    const QString bucket = QJsonDocument::fromJson(json).object().value("created_bucket").toString();
    EXPECT_TRUE(QRegularExpression("^[0-9]{4}-[0-9]{2}$").match(bucket).hasMatch());
}
