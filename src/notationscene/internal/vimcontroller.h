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

    std::unique_ptr<QProcess> m_engine;
    int m_lastKey = -1;
    bool m_lastConsumed = false;
};
}
