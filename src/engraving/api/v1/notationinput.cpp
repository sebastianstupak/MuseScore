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
#include "notationinput.h"

namespace mu::engraving::apiv1 {
NotationInput::NotationInput(QObject* parent)
    : QObject(parent)
{
}

NotationInput::~NotationInput()
{
    stop();
}

void NotationInput::setKeyHandler(QJSValue cb)
{
    m_cb = cb;

    interceptor()->setHandler([this](const mu::context::RawKeyEvent& e, bool wouldConsumePhase) -> bool {
        if (!m_cb.isCallable()) {
            return false;
        }

        QJSValueList args{ QJSValue(e.key), QJSValue(e.modifiers), QJSValue(e.text), QJSValue(wouldConsumePhase) };
        QJSValue r = m_cb.call(args);
        return r.isBool() ? r.toBool() : false;
    });
}

void NotationInput::stop()
{
    m_cb = QJSValue();
    interceptor()->clearHandler();
}
}
