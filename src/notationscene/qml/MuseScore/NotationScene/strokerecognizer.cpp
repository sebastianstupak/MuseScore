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

#include "strokerecognizer.h"

#include <algorithm>
#include <cmath>

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QDate>

#include "notation/inotationinteraction.h"
#include "notation/inotationnoteinput.h"
#include "notation/inotationelements.h"
#include "notation/inotationselection.h"

#include "notation/internal/correctionrecord.h"
#include "notation/internal/correctioncontext.h"
#include "notation/internal/correctiontaxonomy.h"

#include "engraving/types/types.h"
#include "engraving/dom/factory.h"
#include "engraving/dom/score.h"
#include "engraving/dom/accidental.h"
#include "engraving/dom/clef.h"

#include "log.h"

using namespace mu::notation;
using namespace muse;

// 4.7 port: INotationNoteInput::setDuration(DurationType) is 5.0-only. 4.7 sets
// the note-input duration through padNote(Pad), the same path the duration
// buttons in the note-input toolbar use, so the recognizer maps straight to a
// Pad instead of a DurationType.
static mu::engraving::Pad padFromString(const QString& d)
{
    using P = mu::engraving::Pad;
    if (d == "whole") {
        return P::NOTE1;
    } else if (d == "half") {
        return P::NOTE2;
    } else if (d == "eighth") {
        return P::NOTE8;
    } else if (d == "16th") {
        return P::NOTE16;
    } else if (d == "32nd") {
        return P::NOTE32;
    } else if (d == "64th") {
        return P::NOTE64;
    }
    return P::NOTE4;
}

static void addUnique(std::vector<EngravingItem*>& out, EngravingItem* el)
{
    if (el && std::find(out.begin(), out.end(), el) == out.end()) {
        out.push_back(el);
    }
}

// Elements directly under a scribble path (for erase).
static std::vector<EngravingItem*> collectAlongPath(const QList<QPointF>& path, const INotationInteractionPtr& interaction, float width)
{
    std::vector<EngravingItem*> out;
    for (const QPointF& p : path) {
        addUnique(out, interaction->hitElement(muse::PointF(p.x(), p.y()), width));
    }
    return out;
}

static bool pointInPolygon(const std::vector<muse::PointF>& poly, double x, double y)
{
    bool inside = false;
    const size_t n = poly.size();
    for (size_t i = 0, j = n - 1; i < n; j = i++) {
        const double xi = poly[i].x(), yi = poly[i].y();
        const double xj = poly[j].x(), yj = poly[j].y();
        const bool intersect = ((yi > y) != (yj > y))
                               && (x < (xj - xi) * (y - yi) / (yj - yi + 1e-9) + xi);
        if (intersect) {
            inside = !inside;
        }
    }
    return inside;
}

// Elements enclosed by a lasso polygon: probe a grid of interior points and hit-test.
static std::vector<EngravingItem*> collectInPolygon(const QList<QPointF>& path, const INotationInteractionPtr& interaction, double spatium)
{
    std::vector<muse::PointF> poly;
    double minX = 1e18, minY = 1e18, maxX = -1e18, maxY = -1e18;
    for (const QPointF& p : path) {
        const double x = p.x();
        const double y = p.y();
        poly.emplace_back(x, y);
        minX = std::min(minX, x);
        minY = std::min(minY, y);
        maxX = std::max(maxX, x);
        maxY = std::max(maxY, y);
    }

    std::vector<EngravingItem*> out;
    if (poly.size() < 3) {
        return out;
    }

    double step = spatium > 0.0 ? spatium : 1.0;
    // Cap the grid so a huge lasso can't spin forever.
    const double cells = ((maxX - minX) / step) * ((maxY - minY) / step);
    if (cells > 20000.0) {
        step = std::sqrt((maxX - minX) * (maxY - minY) / 20000.0);
    }

    for (double gy = minY; gy <= maxY; gy += step) {
        for (double gx = minX; gx <= maxX; gx += step) {
            if (pointInPolygon(poly, gx, gy)) {
                addUnique(out, interaction->hitElement(muse::PointF(gx, gy), float(step)));
            }
        }
    }
    return out;
}

// Build the named accidental/clef and drop it at a canvas point (the palette
// drag-drop path). The template element is only needed to produce the drop data.
static bool applyDrop(const QString& element, const muse::PointF& pos,
                      const INotationInteractionPtr& interaction, mu::engraving::Score* score)
{
    if (!score) {
        return false;
    }

    mu::engraving::EngravingItem* el = nullptr;
    if (element == "sharp" || element == "flat" || element == "natural" || element == "double_sharp") {
        mu::engraving::Accidental* acc = mu::engraving::Factory::createAccidental(score->dummy());
        mu::engraving::AccidentalType t = mu::engraving::AccidentalType::SHARP;
        if (element == "flat") {
            t = mu::engraving::AccidentalType::FLAT;
        } else if (element == "natural") {
            t = mu::engraving::AccidentalType::NATURAL;
        } else if (element == "double_sharp") {
            t = mu::engraving::AccidentalType::SHARP2;
        }
        acc->setAccidentalType(t);
        el = acc;
    } else if (element == "g_clef" || element == "f_clef" || element == "c_clef") {
        mu::engraving::Clef* clef = static_cast<mu::engraving::Clef*>(
            mu::engraving::Factory::createItem(mu::engraving::ElementType::CLEF, score->dummy()));
        mu::engraving::ClefType t = mu::engraving::ClefType::G;
        if (element == "f_clef") {
            t = mu::engraving::ClefType::F;
        } else if (element == "c_clef") {
            t = mu::engraving::ClefType::C3;
        }
        clef->setClefType(t);
        el = clef;
    }

    if (!el) {
        return false;
    }

    const QByteArray edata = el->mimeData().toQByteArray();
    interaction->startDropSingle(edata);
    interaction->updateDropSingle(pos, Qt::NoModifier);   // snaps to the nearest target
    const bool ok = interaction->dropSingle(pos, Qt::NoModifier);
    interaction->endDrop();
    delete el;   // template only; the interaction created its own element from edata
    return ok;
}

QByteArray StrokeRecognizer::runNeume(const QByteArray& inputJson) const
{
    QString bin = QString::fromLocal8Bit(qgetenv("STYLUS_NEUME_BIN"));
    if (bin.isEmpty()) {
        // Fall back to a `neume` binary shipped next to the MuseScore executable.
#ifdef Q_OS_WIN
        const QString exeName = "/neume.exe";
#else
        const QString exeName = "/neume";
#endif
        const QString local = QCoreApplication::applicationDirPath() + exeName;
        if (QFile::exists(local)) {
            bin = local;
        }
    }
    if (bin.isEmpty()) {
        LOGW() << "stylus: neume binary not found (set STYLUS_NEUME_BIN or ship neume next to the app)";
        return QByteArray();
    }

    QProcess proc;
    proc.start(bin, QStringList());
    if (!proc.waitForStarted(3000)) {
        LOGE() << "stylus: failed to start neume: " << bin;
        return QByteArray();
    }
    proc.write(inputJson);
    proc.closeWriteChannel();
    if (!proc.waitForFinished(5000)) {
        LOGE() << "stylus: neume timed out";
        proc.kill();
        return QByteArray();
    }
    return proc.readAllStandardOutput();
}

int StrokeRecognizer::recognizeAndApply(const std::vector<std::vector<muse::PointF> >& strokes, const INotationPtr& notation,
                                        double spatium, bool additive)
{
    if (strokes.empty() || !notation) {
        return 0;
    }

    // Build {unit, strokes:[{samples:[{x,y}...]}, ...]} for neume.
    QJsonArray strokesJson;
    for (const std::vector<muse::PointF>& stroke : strokes) {
        QJsonArray samples;
        for (const muse::PointF& p : stroke) {
            QJsonObject s;
            s["x"] = p.x();
            s["y"] = p.y();
            samples.append(s);
        }
        QJsonObject strokeObj;
        strokeObj["samples"] = samples;
        strokesJson.append(strokeObj);
    }

    QJsonObject root;
    root["unit"] = spatium > 0.0 ? spatium : 1.0;
    root["strokes"] = strokesJson;

    const QByteArray output = runNeume(QJsonDocument(root).toJson(QJsonDocument::Compact));
    if (output.isEmpty()) {
        return 0;
    }

    // Parse into typed results (carries label/confidence/top-k for the flywheel).
    const QList<RecognizerResult> results = parseRecognizerOutput(output);
    if (results.isEmpty()) {
        return 0;
    }

    INotationInteractionPtr interaction = notation->interaction();
    if (!interaction) {
        return 0;
    }
    INotationNoteInputPtr noteInput = interaction->noteInput();

    int applied = 0;
    for (const RecognizerResult& r : results) {
        mu::engraving::EngravingItem* created = nullptr;

        if (r.type == "put_note" && noteInput) {
            const muse::PointF pos(r.at.x(), r.at.y());
            if (!noteInput->isNoteInputMode()) {
                // Seat the selection where the pen is BEFORE entering note
                // input. startNoteInput() calls
                // resolveNoteInputStartPosition(), which falls back to the
                // current selection or the first element in the score, and
                // then selects it -- and selecting scrolls. Draw a note in
                // bar 40 with nothing selected and the view jumped to bar
                // 1, which is the "view jumps sometimes" people see: it
                // happens on the FIRST note of a session and after
                // anything that clears the selection, not on every note.
                if (mu::engraving::EngravingItem* under = interaction->hitElement(pos, 10.0)) {
                    interaction->select({ under }, mu::engraving::SelectType::SINGLE);
                }
                noteInput->startNoteInput();   // enter note input; pitch derived from pos.y
            }
            noteInput->padNote(padFromString(r.duration));
            noteInput->putNote(pos, /*replace*/ false, /*insert*/ false);
            created = interaction->selection() ? interaction->selection()->element() : nullptr;
            ++applied;
        } else if (r.type == "erase") {
            const float width = float(spatium > 0.0 ? spatium : 1.0);
            std::vector<EngravingItem*> elems = collectAlongPath(r.path, interaction, width);
            if (!elems.empty()) {
                interaction->select(elems);          // REPLACE (default)
                interaction->deleteSelection();      // one undo command
                ++applied;
            }
        } else if (r.type == "lasso") {
            std::vector<EngravingItem*> elems = collectInPolygon(r.path, interaction, spatium);
            if (!elems.empty()) {
                // additive -> accumulate across loops; otherwise replace.
                interaction->select(elems, additive ? mu::engraving::SelectType::ADD : mu::engraving::SelectType::REPLACE);
                ++applied;
            }
        } else if (r.type == "drop") {
            const muse::PointF pos(r.at.x(), r.at.y());
            mu::engraving::Score* score = notation->elements() ? notation->elements()->msScore() : nullptr;
            if (applyDrop(r.element, pos, interaction, score)) {
                created = interaction->selection() ? interaction->selection()->element() : nullptr;
                ++applied;
            }
        } else {
            LOGI() << "stylus: intent '" << r.type << "' not applied";
        }

        // Flywheel: auto-log an "accepted" sample for a classified symbol we applied.
        // (Unverified positive — the design's default-on capture; explicit
        // tap-to-correct will later produce "corrected"/"confirmed" records.)
        if (created && !r.label.isEmpty()) {
            if (const TaxonEntry* tx = taxonByHomusLabel(r.label)) {
                CorrectionContext ctx = extractContext(created);
                RecordMeta meta;
                meta.createdBucket = QDate::currentDate().toString("yyyy-MM");
                meta.appVersion = QCoreApplication::applicationVersion();
                meta.unit = spatium;
                QList<QList<QPointF> > qstrokes;
                for (const std::vector<muse::PointF>& s : strokes) {
                    QList<QPointF> one;
                    for (const muse::PointF& p : s) {
                        one.append(QPointF(p.x(), p.y()));
                    }
                    qstrokes.append(one);
                }
                recordCorrection(buildCorrectionRecord(qstrokes, r, ctx, "accepted",
                                                       taxonLabelObject(*tx), /*inModelVocab*/ true, meta));
            }
        }
    }

    return applied;
}

bool StrokeRecognizer::recordCorrection(const QByteArray& recordJson) const
{
    QString bin = QString::fromLocal8Bit(qgetenv("STYLUS_NEUME_BIN"));
    if (bin.isEmpty()) {
#ifdef Q_OS_WIN
        const QString local = QCoreApplication::applicationDirPath() + "/neume.exe";
#else
        const QString local = QCoreApplication::applicationDirPath() + "/neume";
#endif
        if (QFile::exists(local)) {
            bin = local;
        }
    }
    if (bin.isEmpty()) {
        LOGW() << "stylus: neume binary not found; correction not recorded";
        return false;
    }

    QProcess proc;
    proc.start(bin, QStringList() << "record");
    if (!proc.waitForStarted(3000)) {
        LOGE() << "stylus: failed to start neume record";
        return false;
    }
    proc.write(recordJson);
    proc.closeWriteChannel();
    if (!proc.waitForFinished(5000)) {
        LOGE() << "stylus: neume record timed out";
        proc.kill();
        return false;
    }
    return QJsonDocument::fromJson(proc.readAllStandardOutput()).object().value("ok").toBool(false);
}
