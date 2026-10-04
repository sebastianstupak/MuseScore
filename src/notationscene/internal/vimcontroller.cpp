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
#include <QMap>
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
#include "engraving/dom/noteval.h"
#include "engraving/editing/noteinput.h"
#include "engraving/editing/transaction/transaction.h"

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
    m_trace = !qEnvironmentVariableIsEmpty("MUSE_VIM_TRACE") || !qEnvironmentVariableIsEmpty("MUSE_VIM_SELFTEST");
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

    // Bootstrap: select the first actual NOTE so selection-dependent ops
    // (chord-build, j/k navigation) have something to act on. In real use the
    // user clicks a note first; a headless run has no initial selection.
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

    // ,M<CR> builds a major triad on the selected note (chord-build, direct
    // engraving — works headless). Then k/j navigate the resulting 3-note
    // chord. (chord-drop needs note-input which is focus-gated, so it is only
    // exercised in the user's focused GUI, not here.)
    struct K { int key; const char* text; };
    static const K seq[] = {
        { Qt::Key_Comma, "," },       // leader
        { Qt::Key_M, "M" },           // major quality
        { Qt::Key_Return, "" },       // commit root-position triad -> chord-build:0,4,7
        { Qt::Key_K, "k" },           // select note above within the new chord
        { Qt::Key_J, "j" },           // select note below
    };
    // Feed each key through BOTH phases (ShortcutOverride then KeyPress), exactly
    // as the qApp event filter does in the real GUI, so the two-phase dedupe path
    // is exercised headless (not just the SO path).
    for (const K& k : seq) {
        context::RawKeyEvent ev;
        ev.key = k.key;
        ev.modifiers = Qt::NoModifier;
        ev.text = QString::fromUtf8(k.text);
        ev.autoRepeat = false;
        LOGI() << "VimController SELFTEST: feed key '" << k.text << "' (SO+KP)";
        onKey(ev, true);    // ShortcutOverride phase
        onKey(ev, false);   // KeyPress phase
    }

    // Report the chord the selection sits in — verifies chord-build placed the
    // triad tones (expect 3 notes; e.g. a major triad on pitch P = P, P+4, P+7).
    {
        using namespace mu::engraving;
        if (auto notation = globalContext()->currentNotation()) {
            EngravingItem* el = notation->interaction()->selection()->element();
            if (el && el->isNote()) {
                Chord* c = toNote(el)->chord();
                QStringList ps;
                for (Note* cn : c->notes()) {
                    ps << QString::number(cn->pitch());
                }
                LOGI() << "VimController SELFTEST: selected chord has " << c->notes().size()
                       << " notes, pitches [" << ps.join(",").toStdString() << "]";
            }
        }
    }
    LOGI() << "VimController SELFTEST: end";
}

bool VimController::onKey(const context::RawKeyEvent& e, bool shortcutOverridePhase)
{
    // Scope: only act when a score/notation is the current document.
    if (!globalContext()->currentNotation()) {
        return false;
    }

    if (m_trace) {
        LOGI() << "VimController::onKey " << (shortcutOverridePhase ? "SO" : "KP")
               << " key=" << e.key << " text='" << e.text.toStdString()
               << "' mods=" << e.modifiers << " autoRepeat=" << e.autoRepeat
               << " m_lastKey=" << m_lastKey;
    }

    if (shortcutOverridePhase) {
        QStringList cmds;
        const bool consumed = feedEngine(e, cmds);
        m_lastKey = e.key;
        m_lastConsumed = consumed;
        if (m_trace) {
            LOGI() << "VimController::onKey   SO -> consumed=" << consumed
                   << " cmds=[" << cmds.join("|").toStdString() << "]";
        }
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
        if (m_trace) {
            LOGI() << "VimController::onKey   KP -> deduped (cached consumed=" << c << ")";
        }
        return c;
    }
    QStringList cmds;
    const bool consumed = feedEngine(e, cmds);
    if (m_trace) {
        LOGI() << "VimController::onKey   KP -> fed, consumed=" << consumed
               << " cmds=[" << cmds.join("|").toStdString() << "]";
    }
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

void VimController::buildChordOnSelection(const QString& offsetsCsv)
{
    using namespace mu::engraving;

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
        LOGW() << "VimController: chord-build needs a single selected note";
        return;
    }
    Note* anchor = toNote(el);
    Chord* chord = anchor->chord();
    Score* score = anchor->score();
    if (!chord || !score) {
        return;
    }

    LOGI() << "VimController: chord-build offsets=" << offsetsCsv.toStdString();

    notation->undoStack()->prepareChanges(muse::TranslatableString("undoableAction", "Vim: build chord"));
    Transaction& tx = score->transactionManager()->currentOrDummyTransaction();
    for (const QString& tok : offsetsCsv.split(',', Qt::SkipEmptyParts)) {
        bool ok = false;
        const int off = tok.toInt(&ok);
        const int pitch = anchor->pitch() + off;
        // Skip off==0 (the anchor itself) and any pitch already in the chord
        // (spec §10: merge, don't double) — otherwise addNote creates a unison
        // duplicate notehead, e.g. ,M then ,m on the same note would double the 5th.
        if (!ok || off == 0 || chord->findNote(pitch)) {
            continue;
        }
        NoteVal nv(pitch); // Phase 1: MIDI pitch only; tpc1 stays TPC_INVALID -> default spelling
        NoteInput::addNote(tx, score, chord, nv);
    }
    notation->undoStack()->commitChanges();

    interaction->select({ anchor }, SelectType::SINGLE);
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
        } else if (head == "chord-build") {
            buildChordOnSelection(parts.value(1));
        } else if (head == "chord-drop") {
            // Place the anchor note by letter via native note input, then build
            // the chord around it (the just-entered note becomes the selection).
            const QString letter = parts.value(1);
            const QString csv = parts.value(2);
            static const QMap<QString, std::string> kNoteAction = {
                { "a", "note-a" }, { "b", "note-b" }, { "c", "note-c" }, { "d", "note-d" },
                { "e", "note-e" }, { "f", "note-f" }, { "g", "note-g" }
            };
            if (kNoteAction.contains(letter)) {
                dispatchCode(kNoteAction.value(letter));
                buildChordOnSelection(csv);
            } else {
                LOGW() << "VimController: chord-drop bad pitch letter " << letter.toStdString();
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
