/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-Studio-CLA-applies
 *
 * MuseScore Studio
 * Music Composition & Notation
 *
 * Copyright (C) 2026 MuseScore Limited and others
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
#include "vimcontroller.h"

#include <cstdlib>

#include <QProcess>
#include <QKeyEvent>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

#include "notation/inotation.h"
#include "notation/inotationinteraction.h"
#include "notation/inotationselection.h"
#include "notation/inotationelements.h"
#include "engraving/dom/note.h"
#include "engraving/dom/chord.h"
#include "engraving/dom/segment.h"
#include "engraving/dom/score.h"
#include "engraving/dom/mscore.h"

#include "log.h"

using namespace mu::notation;

// Dev spike: path to the external Rust vim-engine CLI binary. Override with the
// MUSE_VIM_ENGINE env var; otherwise the dev build location is used. If the
// binary can't be started, Vim stays disabled (no handler registered).
static QString enginePath()
{
    if (const char* p = std::getenv("MUSE_VIM_ENGINE")) {
        return QString::fromUtf8(p);
    }
    return QStringLiteral("D:/dev/notation/motus/target/release/vim-engine.exe");
}

VimController::VimController(const muse::modularity::ContextPtr& iocCtx)
    : muse::Contextable(iocCtx)
{
}

VimController::~VimController()
{
    if (m_engine) {
        m_engine->closeWriteChannel();
        m_engine->kill();
        m_engine->waitForFinished(500);
    }
}

void VimController::init()
{
    const QString path = enginePath();
    m_engine = std::make_unique<QProcess>();
    m_engine->setProgram(path);
    m_engine->start();
    if (!m_engine->waitForStarted(2000)) {
        LOGW() << "VimController: could not start vim-engine at " << path.toStdString() << " — Vim disabled";
        m_engine.reset();
        return;
    }
    LOGI() << "VimController: vim-engine started; registering key interceptor";
    interceptor()->setHandler([this](const context::RawKeyEvent& e, bool phase) {
        return this->onKey(e, phase);
    });

    maybeScheduleSelfTest();
}

void VimController::maybeScheduleSelfTest()
{
    if (qEnvironmentVariableIsEmpty("MUSE_VIM_SELFTEST")) {
        return;
    }
    // Poll (on the UI thread) until a score is open, then run the sequence once.
    // Opening a legacy-format score triggers a migration that can take ~30s to
    // auto-complete headlessly, so wait generously (150 * 400ms = 60s).
    if (globalContext()->currentNotation()) {
        runSelfTest();
        return;
    }
    if (++m_selfTestTries > 150) {
        LOGW() << "VimController SELFTEST: no score opened after waiting; skipping";
        return;
    }
    if (m_selfTestTries % 25 == 0) {
        LOGI() << "VimController SELFTEST: waiting for a score to open (try " << m_selfTestTries << ")";
    }
    QTimer::singleShot(400, [this]() { maybeScheduleSelfTest(); });
}

void VimController::runSelfTest()
{
    LOGI() << "VimController SELFTEST: begin (feeding fixed sequence via onKey)";

    // Bootstrap: select the first actual NOTE so selection-dependent ops (the
    // triad, j/k navigation) have something to act on. In real use the user
    // clicks a note first; a headless run has no initial selection.
    {
        using namespace mu::engraving;
        if (auto notation = globalContext()->currentNotation()) {
            Score* score = notation->elements() ? notation->elements()->msScore() : nullptr;
            Note* firstNote = nullptr;
            for (Segment* s = score ? score->firstSegment(SegmentType::ChordRest) : nullptr;
                 s && !firstNote; s = s->next1(SegmentType::ChordRest)) {
                for (track_idx_t t = 0; t < score->ntracks(); ++t) {
                    EngravingItem* e = s->element(t);
                    if (e && e->isChord()) {
                        firstNote = toChord(e)->downNote();
                        break;
                    }
                }
            }
            if (firstNote) {
                // Select directly (not via a context-gated action) so this works
                // in a headless run where the notation view isn't focused.
                notation->interaction()->select({ firstNote }, SelectType::SINGLE);
                LOGI() << "VimController SELFTEST: selected first note, pitch " << firstNote->pitch();
            } else {
                LOGW() << "VimController SELFTEST: no note found to select";
            }
        }
    }

    struct K { int key; const char* text; };
    static const K seq[] = {
        { Qt::Key_L, "l" },           // -> notation-move-right (select a note)
        { Qt::Key_L, "l" },           // -> notation-move-right
        { Qt::Key_H, "h" },           // -> notation-move-left
        { Qt::Key_Comma, "," },       // -> (leader pending)
        { Qt::Key_T, "t" },           // -> interval3 + interval5 (triad on selection)
        { Qt::Key_J, "j" },           // -> select note below in the new chord
        { Qt::Key_K, "k" },           // -> select note above in the new chord
        { Qt::Key_X, "x" },           // -> action://delete
    };
    for (const K& k : seq) {
        context::RawKeyEvent ev;
        ev.key = k.key;
        ev.modifiers = Qt::NoModifier;
        ev.text = QString::fromUtf8(k.text);
        ev.autoRepeat = false;
        m_lastKey = -1; // bypass the phase-dedupe path
        LOGI() << "VimController SELFTEST: feed key '" << k.text << "'";
        const bool consumed = onKey(ev, true);
        LOGI() << "VimController SELFTEST:   -> consumed=" << (consumed ? "true" : "false");
    }
    LOGI() << "VimController SELFTEST: end";
}

bool VimController::onKey(const context::RawKeyEvent& e, bool shortcutOverridePhase)
{
    // Scope: only act when a score/notation is the current document.
    if (!globalContext()->currentNotation()) {
        return false;
    }

    if (shortcutOverridePhase) {
        QStringList cmds;
        const bool consumed = feedEngine(e, cmds);
        m_lastKey = e.key;
        m_lastConsumed = consumed;
        if (consumed) {
            applyOps(cmds);
        }
        return consumed;
    }

    // KeyPress phase: reuse the ShortcutOverride decision for the same key to
    // avoid double-feeding; feed here only for keys that had no ShortcutOverride.
    if (e.key == m_lastKey) {
        const bool c = m_lastConsumed;
        m_lastKey = -1;
        return c;
    }
    QStringList cmds;
    const bool consumed = feedEngine(e, cmds);
    if (consumed) {
        applyOps(cmds);
    }
    return consumed;
}

bool VimController::feedEngine(const context::RawKeyEvent& e, QStringList& outCmds)
{
    if (!m_engine) {
        return false;
    }

    int bits = 0;
    const int m = e.modifiers;
    if (m & Qt::ControlModifier) {
        bits |= 1;
    }
    if (m & Qt::ShiftModifier) {
        bits |= 2;
    }
    if (m & Qt::AltModifier) {
        bits |= 4;
    }
    if (m & Qt::MetaModifier) {
        bits |= 8;
    }

    QJsonObject in;
    in["key"] = e.key;
    in["mods"] = bits;
    in["text"] = e.text;
    QByteArray line = QJsonDocument(in).toJson(QJsonDocument::Compact);
    line.append('\n');

    m_engine->write(line);
    m_engine->waitForBytesWritten(50);
    if (!m_engine->waitForReadyRead(200)) {
        LOGW() << "VimController: no reply from vim-engine";
        return false;
    }
    const QByteArray reply = m_engine->readLine();
    const QJsonObject out = QJsonDocument::fromJson(reply).object();
    const bool consumed = out.value("consumed").toBool();
    const QJsonArray arr = out.value("cmds").toArray();
    for (const QJsonValue& v : arr) {
        outCmds << v.toString();
    }
    return consumed;
}

void VimController::dispatchCode(const std::string& code, int times)
{
    for (int i = 0; i < times && i < 1000; ++i) {
        dispatcher()->dispatch(code);
    }
}

void VimController::moveChordNote(bool up, int times)
{
    using namespace mu::engraving;
    if (times <= 0) {
        times = 1;
    }
    LOGI() << "VimController: moveChordNote " << (up ? "up" : "down") << " x" << times;

    for (int step = 0; step < times && step < 1000; ++step) {
        auto notation = globalContext()->currentNotation();
        if (!notation) {
            return;
        }
        auto interaction = notation->interaction();
        if (!interaction) {
            return;
        }
        EngravingItem* el = interaction->selection()->element();
        if (!el || !el->isNote()) {
            LOGW() << "VimController: j/k needs a single selected note";
            return;
        }
        Note* note = toNote(el);
        Chord* chord = note->chord();
        const int curPitch = note->pitch();

        // 1) Nearest note in the wanted direction within the same chord.
        EngravingItem* target = nullptr;
        int bestDelta = 0;
        for (Note* n : chord->notes()) {
            if (n == note) {
                continue;
            }
            const int d = n->pitch() - curPitch;
            if (up && d > 0 && (!target || d < bestDelta)) {
                target = n;
                bestDelta = d;
            } else if (!up && d < 0 && (!target || d > bestDelta)) {
                target = n;
                bestDelta = d;
            }
        }

        // 2) At the top/bottom of the chord -> cross to the staff above/below
        //    at the same tick, entering at its nearest (bottom/top) note.
        if (!target) {
            Segment* seg = chord->segment();
            Score* score = note->score();
            if (!seg || !score) {
                return;
            }
            const staff_idx_t staffIdx = chord->staffIdx();
            if (up && staffIdx == 0) {
                return; // already the top staff
            }
            const staff_idx_t targetStaff = up ? staffIdx - 1 : staffIdx + 1;
            if (targetStaff >= score->nstaves()) {
                return; // no staff below
            }
            EngravingItem* found = nullptr;
            for (size_t v = 0; v < VOICES; ++v) {
                if (EngravingItem* e = seg->element(targetStaff * VOICES + v)) {
                    found = e;
                    break;
                }
            }
            if (!found) {
                return;
            }
            if (found->isChord()) {
                Chord* c = toChord(found);
                target = up ? c->downNote() : c->upNote();
            } else {
                target = found; // rest or similar -> select it directly
            }
        }

        if (!target) {
            return;
        }
        interaction->select({ target }, SelectType::SINGLE);
        LOGI() << "VimController: j/k selected element at track " << target->track();
    }
}

void VimController::applyOps(const QStringList& cmds)
{
    for (const QString& cmd : cmds) {
        const QStringList parts = cmd.split(':');
        const QString head = parts.value(0);

        if (head == "move") {
            const QString target = parts.value(1);
            int n = parts.value(2).toInt();
            if (n <= 0) {
                n = 1;
            }
            if (target == "next-chord") {
                dispatchCode("notation-move-right", n);
            } else if (target == "prev-chord") {
                dispatchCode("notation-move-left", n);
            } else if (target == "next-measure") {
                dispatchCode("notation-move-right-quickly", n);
            } else if (target == "prev-measure") {
                dispatchCode("notation-move-left-quickly", n);
            } else if (target == "score-start") {
                dispatchCode("first-element");
            } else if (target == "score-end") {
                dispatchCode("last-element");
            } else if (target == "within-chord-up") {
                // Vertical k: SELECT the note above within the chord (staff
                // above at the top). A motion, not an edit.
                moveChordNote(true, n);
            } else if (target == "within-chord-down") {
                // Vertical j: select the note below (staff below at the bottom).
                moveChordNote(false, n);
            } else {
                // measure-start (0) / measure-end ($) have no clean native
                // navigation action yet — honest known gap, left unmapped.
                LOGW() << "VimController: unmapped move target " << target.toStdString();
            }
        } else if (cmd == "insert.enter" || cmd == "insert.exit") {
            dispatchCode("note-input");
        } else if (head == "chordAdd") {
            const QString iv = parts.value(1);
            if (iv == "third") {
                dispatchCode("interval3");
            } else if (iv == "fifth") {
                dispatchCode("interval5");
            } else if (iv == "seventh") {
                dispatchCode("interval7");
            }
        } else if (cmd == "delete:element") {
            dispatchCode("action://delete");
        } else if (cmd == "delete:measure") {
            dispatchCode("time-delete");
        } else if (cmd == "undo") {
            dispatchCode("action://undo");
        } else if (cmd == "redo") {
            dispatchCode("action://redo");
        } else if (head == "raw") {
            dispatchCode(cmd.mid(4).toStdString());
        } else {
            LOGW() << "VimController: unmapped op " << cmd.toStdString();
        }
    }
}
