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
#pragma once

#include <QObject>

#include "../iinputinterceptor.h"

namespace mu::context {
//! Default IInputInterceptor impl.
//! When a handler is registered it installs a qApp-level event filter so it can
//! see (and optionally consume) keys regardless of which item/window holds focus
//! — necessary because notation keys are dispatched by window-scoped QML
//! Shortcut elements, not via the focused paint view. Modeled on the existing
//! InitialLetterNavigation qApp event filter. Default-off: no handler ⇒ no filter.
class InputInterceptor : public QObject, public IInputInterceptor
{
    Q_OBJECT
public:
    InputInterceptor() = default;
    ~InputInterceptor() override;

    void setHandler(Handler h) override;
    void clearHandler() override;
    bool hasHandler() const override;

    bool wouldConsume(const RawKeyEvent& e) override;
    bool handleKey(const RawKeyEvent& e) override;

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void installFilter();
    void removeFilter();

    Handler m_handler;
    bool m_filterInstalled = false;
};
}
