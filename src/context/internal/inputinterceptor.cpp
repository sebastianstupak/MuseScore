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
#include "inputinterceptor.h"

using namespace mu::context;

void InputInterceptor::setHandler(Handler h)
{
    m_handler = std::move(h);
}

void InputInterceptor::clearHandler()
{
    m_handler = nullptr;
}

bool InputInterceptor::hasHandler() const
{
    return static_cast<bool>(m_handler);
}

bool InputInterceptor::wouldConsume(const RawKeyEvent& e)
{
    return m_handler ? m_handler(e, true) : false;
}

bool InputInterceptor::handleKey(const RawKeyEvent& e)
{
    return m_handler ? m_handler(e, false) : false;
}
