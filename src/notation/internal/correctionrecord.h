/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 */
#ifndef MU_NOTATION_CORRECTIONRECORD_H
#define MU_NOTATION_CORRECTIONRECORD_H

#include <QByteArray>
#include <QList>
#include <QPointF>
#include <QString>

#include "correctioncontext.h"

class QJsonObject;

namespace mu::notation {
struct RecTopk {
    QString label;
    double p = 0.0;
};

// One recognized+applied intent from neume, with the detail the flywheel needs.
struct RecognizerResult {
    QString type;               // "put_note" | "drop" | "erase" | "lasso"
    QString label;              // HOMUS class string; empty for geometry actions
    double confidence = 1.0;
    QList<RecTopk> topk;
    QList<int> strokeIndices;
    QPointF at;                 // put_note / drop anchor
    QString duration;           // put_note
    QString element;            // drop ("sharp", "g_clef", ...)
    QList<QPointF> path;        // erase / lasso
};

// Parse neume's `{"intents":[...]}` output (A1 contract) into results. Tolerant:
// missing label/confidence/topk/stroke_indices default; bad/empty input -> empty.
QList<RecognizerResult> parseRecognizerOutput(const QByteArray& json);

// Metadata supplied by the caller when assembling a correction record.
struct RecordMeta {
    QString id;             // caller supplies (QUuid WithoutBraces); if empty, buildCorrectionRecord fills one
    QString createdBucket;  // "yyyy-MM"; if empty, filled from QDate::currentDate()
    QString engineVersion;
    QString modelVersion;
    QString appVersion;
    double unit = 0.0;
};

// Assemble the schema-1 correction record JSON sent to `neume record`.
QByteArray buildCorrectionRecord(const QList<QList<QPointF> >& strokes,
                                 const RecognizerResult& recognized,
                                 const CorrectionContext& ctx,
                                 const QString& verdict,
                                 const QJsonObject& label,
                                 bool inModelVocab,
                                 const RecordMeta& meta);
}

#endif // MU_NOTATION_CORRECTIONRECORD_H
