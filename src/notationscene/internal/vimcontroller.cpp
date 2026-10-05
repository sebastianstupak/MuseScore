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
#include <QSet>
#include <QGuiApplication>
#include <QWindow>
#include <QCoreApplication>
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
    vimStatus()->setStatusText("NORMAL"); // seed the footer indicator

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
    using namespace mu::engraving;
    LOGI() << "VimController SELFTEST: begin (UI e2e: real QKeyEvents through the qApp filter)";

    // Each case injects a key sequence exactly as Qt delivers real keys
    // (ShortcutOverride then KeyPress) so the FULL path runs: qApp filter
    // (InputInterceptor) -> onKey -> vim-engine subprocess -> applyOps. The only
    // link not exercised is the OS->Qt key-code translation, which OS-level
    // injection cannot reach in a headless/agent shell.
    //
    // Two kinds of assertion:
    //  - OUTCOME: direct-engraving ops (chord-build, j/k select) actually run
    //    headless, so we assert the resulting score/selection state.
    //  - DISPATCH: context-gated actions (pitch/tie/articulations/durations/
    //    voices/copy/...) no-op without notation focus, but dispatchCode() still
    //    records the code, so we assert the exact action dispatched per keystroke.

    auto notation = globalContext()->currentNotation();
    if (!notation) {
        LOGW() << "VimController SELFTEST: no current notation";
        LOGI() << "VIMSELFTEST: FAIL";
        return;
    }

    QWindow* target = QGuiApplication::focusWindow();
    if (!target) {
        const QList<QWindow*> tops = QGuiApplication::topLevelWindows();
        for (QWindow* w : tops) {
            if (w->isVisible()) {
                target = w;
                break;
            }
        }
        if (!target && !tops.isEmpty()) {
            target = tops.first();
        }
    }
    if (!target) {
        LOGW() << "VimController SELFTEST: no window to inject QKeyEvents into";
        LOGI() << "VIMSELFTEST: FAIL";
        return;
    }

    // Re-find the first NOTE fresh each time (so a mutating action can never
    // leave the suite holding a dangling pointer); cache its pitch once.
    auto findFirstNote = [&]() -> Note* {
        Score* score = notation->elements() ? notation->elements()->msScore() : nullptr;
        for (Segment* s = score ? score->firstSegment(SegmentType::ChordRest) : nullptr;
             s; s = s->next1(SegmentType::ChordRest)) {
            for (track_idx_t t = 0; t < score->ntracks(); ++t) {
                EngravingItem* e = s->element(t);
                if (e && e->isChord()) {
                    return toChord(e)->downNote();
                }
            }
        }
        return nullptr;
    };
    Note* anchor = findFirstNote();
    if (!anchor) {
        LOGW() << "VimController SELFTEST: no note found to anchor on";
        LOGI() << "VIMSELFTEST: FAIL";
        return;
    }
    const int P = anchor->pitch();
    LOGI() << "VimController SELFTEST: anchor note pitch " << P;

    m_inSelfTest = true;

    // --- injection + assertion helpers ---
    // '\r' -> Return (commit), '\x1b' -> Escape; every other char is sent as its
    // literal text with key code 0 (the engine keys on text except for Enter/Esc).
    auto injOne = [&](char ch) {
        int key = 0;
        QString textStr;
        if (ch == '\r') {
            key = Qt::Key_Return;
        } else if (ch == '\x1b') {
            key = Qt::Key_Escape;
        } else {
            textStr = QString(QChar(ch));
        }
        QKeyEvent so(QEvent::ShortcutOverride, key, Qt::NoModifier, textStr);
        QCoreApplication::sendEvent(target, &so);
        QKeyEvent kp(QEvent::KeyPress, key, Qt::NoModifier, textStr);
        QCoreApplication::sendEvent(target, &kp);
    };
    auto inject = [&](const char* seq) {
        for (const char* p = seq; *p; ++p) {
            injOne(*p);
        }
    };
    auto reselect = [&]() {
        if (Note* a = findFirstNote()) {
            notation->interaction()->select({ a }, SelectType::SINGLE);
        }
    };

    int pass = 0, fail = 0;
    QStringList failures;

    auto check = [&](const char* name, bool ok, const QString& detail) {
        if (ok) {
            ++pass;
        } else {
            ++fail;
            failures << QString("%1 (%2)").arg(name, detail);
            LOGW() << "VimController SELFTEST: FAILED " << name << " -- " << detail.toStdString();
        }
    };
    // DISPATCH case: `seq` must produce EXACTLY `expected` dispatched action codes.
    auto expectDispatch = [&](const char* name, const char* seq, const QStringList& expected) {
        injOne('\x1b');            // clear any half-entered engine state (not recorded)
        reselect();
        m_dispatched.clear();
        m_recording = true;
        inject(seq);
        m_recording = false;
        check(name, m_dispatched == expected,
              QString("got [%1] expected [%2]").arg(m_dispatched.join("|"), expected.join("|")));
    };

    // ===== OUTCOME cases first, on a pristine score =====

    // ,M<CR> builds a major triad {P, P+4, P+7} on the selection.
    {
        injOne('\x1b');
        reselect();
        inject(",M\r");
        EngravingItem* el = notation->interaction()->selection()->element();
        QSet<int> ps;
        if (el && el->isNote()) {
            for (Note* cn : toNote(el)->chord()->notes()) {
                ps.insert(cn->pitch());
            }
        }
        QStringList got;
        for (int x : ps) {
            got << QString::number(x);
        }
        check("chord ,M<CR> builds major triad",
              ps.contains(P) && ps.contains(P + 4) && ps.contains(P + 7),
              QString("pitches=[%1] want {%2,%3,%4}").arg(got.join(",")).arg(P).arg(P + 4).arg(P + 7));
    }

    // j/k navigate WITHIN that chord: k -> P+4 -> P+7, then j -> P+4.
    {
        injOne('\x1b');
        reselect();
        inject(",M\r");   // ensure the triad exists; leaves the anchor (P) selected
        reselect();
        inject("k");
        EngravingItem* a1 = notation->interaction()->selection()->element();
        const int pk1 = (a1 && a1->isNote()) ? toNote(a1)->pitch() : -1;
        inject("k");
        EngravingItem* a2 = notation->interaction()->selection()->element();
        const int pk2 = (a2 && a2->isNote()) ? toNote(a2)->pitch() : -1;
        inject("j");
        EngravingItem* a3 = notation->interaction()->selection()->element();
        const int pj = (a3 && a3->isNote()) ? toNote(a3)->pitch() : -1;
        const QString detail = "k->" + QString::number(pk1) + " k->" + QString::number(pk2)
                               + " j->" + QString::number(pj) + "; want "
                               + QString::number(P + 4) + "," + QString::number(P + 7)
                               + "," + QString::number(P + 4);
        check("j/k navigate within chord", pk1 == P + 4 && pk2 == P + 7 && pj == P + 4, detail);
    }

    // HELP: ? and ,? make the engine return the cheatsheet (observed via the flag).
    {
        injOne('\x1b');
        reselect();
        m_helpRequested = false;
        inject("?");
        check("help ?", m_helpRequested, "? did not request help");
        m_helpRequested = false;
        inject(",?");
        check("help ,?", m_helpRequested, ",? did not request help");
    }

    // ===== DISPATCH cases: assert the exact action code per keystroke =====

    // MOTION
    expectDispatch("motion l", "l", { "notation-move-right" });
    expectDispatch("motion h", "h", { "notation-move-left" });
    expectDispatch("motion w", "w", { "notation-move-right-quickly" });
    expectDispatch("motion b", "b", { "notation-move-left-quickly" });
    expectDispatch("motion gg", "gg", { "first-element" });
    expectDispatch("motion G", "G", { "last-element" });

    // PITCH / STEMS (non-deleting)
    expectDispatch("pitch K", "K", { "pitch-up" });
    expectDispatch("pitch J", "J", { "pitch-down" });
    expectDispatch("tie t", "t", { "tie" });
    expectDispatch("flip f", "f", { "flip" });

    // ARTICULATIONS (a leader)
    expectDispatch("artic as", "as", { "add-staccato" });
    expectDispatch("artic aa", "aa", { "add-sforzato" });
    expectDispatch("artic at", "at", { "add-tenuto" });
    expectDispatch("artic am", "am", { "add-marcato" });
    expectDispatch("artic al", "al", { "add-slur" });

    // DURATIONS (o leader) + tuplets + dot
    expectDispatch("dur ow", "ow", { "pad-note-1" });
    expectDispatch("dur oh", "oh", { "pad-note-2" });
    expectDispatch("dur oq", "oq", { "pad-note-4" });
    expectDispatch("dur oe", "oe", { "pad-note-8" });
    expectDispatch("dur os", "os", { "pad-note-16" });
    expectDispatch("dur ot", "ot", { "pad-note-32" });
    expectDispatch("dur o.", "o.", { "pad-dot" });
    expectDispatch("tuplet o2", "o2", { "duplet" });
    expectDispatch("tuplet o3", "o3", { "triplet" });
    expectDispatch("tuplet o4", "o4", { "quadruplet" });

    // VOICES (V leader)
    expectDispatch("voice V1", "V1", { "voice-1" });
    expectDispatch("voice V2", "V2", { "voice-2" });
    expectDispatch("voice V3", "V3", { "voice-3" });
    expectDispatch("voice V4", "V4", { "voice-4" });

    // VISUAL: v enters, a motion extends (select-*), an operator applies + exits.
    expectDispatch("visual v l y", "vly", { "select-next-chord", "action://copy" });
    expectDispatch("visual v h d", "vhd", { "select-prev-chord", "action://delete" });
    expectDispatch("visual v w x", "vwx", { "select-next-measure", "action://delete" });

    // EDIT that mutates/deletes -> run last (dispatch asserted; executes only with focus)
    expectDispatch("copy y", "y", { "action://copy" });
    expectDispatch("paste p", "p", { "action://paste" });
    expectDispatch("undo u", "u", { "action://undo" });
    expectDispatch("delete x", "x", { "action://delete" });
    expectDispatch("delete dd", "dd", { "time-delete" });

    // ===== verdict =====
    m_recording = false;
    m_inSelfTest = false;
    const int total = pass + fail;
    for (const QString& f : failures) {
        LOGI() << "VimController SELFTEST:   " << f.toStdString();
    }
    LOGI() << "VIMSELFTEST: " << (fail == 0 ? "PASS" : "FAIL")
           << " (" << pass << "/" << total << " cases)"; // grep marker for the e2e runner
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
        const QString c = v.toString();
        if (c == "help") {
            continue; // handled via the reply's `help` text below, not applyOps
        }
        outCmds << c;
    }
    publishStatus(out.value("mode").toString(), out.value("pending").toString());

    const QString help = out.value("help").toString();
    if (!help.isEmpty()) {
        m_helpRequested = true;         // observed by the self-test
        if (!m_inSelfTest) {
            showHelp(help);             // suppress the live dialog during the automated run
        }
    }
    return consumed;
}

void VimController::showHelp(const QString& text)
{
    // Defer out of the key-event filter (don't open a dialog re-entrantly), then
    // show the engine-provided cheatsheet in a non-blocking info dialog.
    QTimer::singleShot(0, [this, text]() {
        interactive()->info("motus — Vim mode", text.toStdString());
    });
}

void VimController::publishStatus(const QString& mode, const QString& pending)
{
    QString label;
    if (mode == "insert") {
        label = "INSERT";
    } else if (mode == "visual") {
        label = "VISUAL";
    } else if (mode == "operator-pending") {
        label = "O-PEND";
    } else {
        label = "NORMAL";
    }
    vimStatus()->setStatusText(pending.isEmpty() ? label : (label + "  " + pending));
}

void VimController::dispatchCode(const std::string& code, int times)
{
    if (m_recording) {
        m_dispatched << QString::fromStdString(code); // self-test: capture the dispatch
    }
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
