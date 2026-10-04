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
#pragma once

#include <memory>

#include <QString>
#include <QStringList>

#include "modularity/ioc.h"
#include "actions/iactionsdispatcher.h"
#include "context/iglobalcontext.h"
#include "context/iinputinterceptor.h"

class QProcess;

namespace mu::notation {
//! Resident bridge: connects the generic key-interception enabler (context
//! module's IInputInterceptor / qApp filter) to the external Rust vim-engine
//! (stdio JSON subprocess), and applies the engine's returned ops via the
//! notation action dispatcher. Lives for the app lifetime.
class VimController : public muse::Contextable
{
    muse::ContextInject<muse::actions::IActionsDispatcher> dispatcher = { this };
    muse::ContextInject<context::IGlobalContext> globalContext = { this };
    muse::GlobalInject<context::IInputInterceptor> interceptor;

public:
    explicit VimController(const muse::modularity::ContextPtr& iocCtx);
    ~VimController();

    void init();

private:
    bool onKey(const context::RawKeyEvent& e, bool shortcutOverridePhase);
    bool feedEngine(const context::RawKeyEvent& e, QStringList& outCmds);
    void applyOps(const QStringList& cmds);
    void dispatchCode(const std::string& code, int times = 1);

    // Vertical note selection (j/k): select the note above/below within the
    // current chord; at the top/bottom of the chord, cross to the staff
    // above/below. No native MuseScore action does this, so it drives the
    // engraving selection model directly.
    void moveChordNote(bool up, int times);

    // chord-build:<csv-offsets> — add tones (semitone offsets from the
    // selected note's pitch) to the selected note's chord via the engraving
    // NoteInput API, wrapped in a single undoable transaction.
    void buildChordOnSelection(const QString& offsetsCsv);

    // Dev/CI self-test: when MUSE_VIM_SELFTEST is set, once a score is open,
    // feed a fixed key sequence through the real onKey() path and log the
    // dispatched actions. Exercises onKey->engine->applyOps->dispatcher without
    // needing OS key injection (which is blocked in headless/agent shells).
    void maybeScheduleSelfTest();
    void runSelfTest();

    std::unique_ptr<QProcess> m_engine;
    int m_lastKey = -1;
    bool m_lastConsumed = false;
    int m_selfTestTries = 0;
};
}
