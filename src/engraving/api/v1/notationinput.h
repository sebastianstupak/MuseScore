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
#include <QJSValue>

#include "modularity/ioc.h"
#include "context/iinputinterceptor.h"

namespace mu::engraving::apiv1 {
//---------------------------------------------------------
//   @@ NotationInput
///   Allows a plugin to register a JavaScript key handler
///   that is invoked for raw key events before (and, if not
///   consumed, after) the application's own key handling.
//---------------------------------------------------------

class NotationInput : public QObject
{
    Q_OBJECT

    muse::GlobalInject<mu::context::IInputInterceptor> interceptor;

public:
    /// \cond MS_INTERNAL
    explicit NotationInput(QObject* parent = nullptr);
    ~NotationInput() override;

    /// Registers a JavaScript callback to be invoked on key events.
    /// The callback signature is:
    /// function(key: int, mods: int, text: string, wouldConsume: bool) -> bool
    /// Returning true tells the caller the event was consumed.
    Q_INVOKABLE void setKeyHandler(QJSValue cb);

    /// Clears any registered handler.
    Q_INVOKABLE void stop();
    /// \endcond

private:
    QJSValue m_cb;
};
}
