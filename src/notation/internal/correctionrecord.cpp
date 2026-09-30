/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#include "correctionrecord.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QUuid>

using namespace mu::notation;

QList<RecognizerResult> mu::notation::parseRecognizerOutput(const QByteArray& json)
{
    QList<RecognizerResult> out;
    if (json.isEmpty()) {
        return out;
    }
    const QJsonDocument doc = QJsonDocument::fromJson(json);
    if (!doc.isObject()) {
        return out;
    }
    const QJsonArray intents = doc.object().value("intents").toArray();
    for (const QJsonValue& v : intents) {
        const QJsonObject o = v.toObject();
        RecognizerResult r;
        r.type = o.value("type").toString();
        r.label = o.value("label").toString();
        r.confidence = o.value("confidence").toDouble(1.0);
        r.duration = o.value("duration").toString();
        r.element = o.value("element").toString();
        const QJsonObject at = o.value("at").toObject();
        r.at = QPointF(at.value("x").toDouble(), at.value("y").toDouble());
        for (const QJsonValue& tv : o.value("topk").toArray()) {
            const QJsonObject t = tv.toObject();
            r.topk.append(RecTopk{ t.value("label").toString(), t.value("p").toDouble() });
        }
        for (const QJsonValue& iv : o.value("stroke_indices").toArray()) {
            r.strokeIndices.append(iv.toInt());
        }
        for (const QJsonValue& pv : o.value("path").toArray()) {
            const QJsonObject p = pv.toObject();
            r.path.append(QPointF(p.value("x").toDouble(), p.value("y").toDouble()));
        }
        if (!r.type.isEmpty()) {
            out.append(r);
        }
    }
    return out;
}

static QJsonObject contextToJson(const CorrectionContext& c)
{
    QJsonObject o;
    o.insert("clef", c.clef);
    o.insert("key", c.key);
    o.insert("time_sig", c.timeSig);
    o.insert("staff_idx", c.staffIdx);
    o.insert("line", c.line);
    o.insert("pitch", c.pitch);
    o.insert("measure_index", c.measureIndex);
    o.insert("beat_pos", c.beatPos);
    o.insert("beat_remaining", c.beatRemaining);
    o.insert("voice", c.voice);
    o.insert("prev_label", c.prevLabel);
    o.insert("next_label", c.nextLabel);
    return o;
}

QByteArray mu::notation::buildCorrectionRecord(const QList<QList<QPointF> >& strokes,
                                               const RecognizerResult& recognized,
                                               const CorrectionContext& ctx,
                                               const QString& verdict,
                                               const QJsonObject& label,
                                               bool inModelVocab,
                                               const RecordMeta& meta)
{
    QJsonObject root;
    root.insert("schema", 1);
    root.insert("id", meta.id.isEmpty() ? QUuid::createUuid().toString(QUuid::WithoutBraces) : meta.id);
    root.insert("engine_version", meta.engineVersion);
    root.insert("model_version", meta.modelVersion);
    root.insert("app_version", meta.appVersion);
    root.insert("created_bucket", meta.createdBucket.isEmpty()
                ? QDate::currentDate().toString("yyyy-MM") : meta.createdBucket);
    root.insert("unit", meta.unit);

    QJsonArray strokesArr;
    for (const QList<QPointF>& s : strokes) {
        QJsonArray one;
        for (const QPointF& p : s) {
            QJsonObject pt;
            pt.insert("x", p.x());
            pt.insert("y", p.y());
            one.append(pt);
        }
        strokesArr.append(one);
    }
    root.insert("strokes", strokesArr);

    QJsonObject rec;
    rec.insert("label", recognized.label);              // HOMUS class string
    rec.insert("confidence", recognized.confidence);
    rec.insert("intent", recognized.type);
    QJsonArray topk;
    for (const RecTopk& t : recognized.topk) {
        QJsonObject to;
        to.insert("label", t.label);
        to.insert("p", t.p);
        topk.append(to);
    }
    rec.insert("topk", topk);
    root.insert("recognized", rec);

    root.insert("verdict", verdict);
    root.insert("label", label);                        // taxonomy object (ground truth)
    root.insert("in_model_vocab", inModelVocab);
    root.insert("context", contextToJson(ctx));

    return QJsonDocument(root).toJson(QJsonDocument::Compact);
}
