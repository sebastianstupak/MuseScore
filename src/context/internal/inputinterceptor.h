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

#include "../iinputinterceptor.h"

namespace mu::context {
class InputInterceptor : public IInputInterceptor
{
public:
    InputInterceptor() = default;

    void setHandler(Handler h) override;
    void clearHandler() override;
    bool hasHandler() const override;

    bool wouldConsume(const RawKeyEvent& e) override;
    bool handleKey(const RawKeyEvent& e) override;

private:
    Handler m_handler;
};
}
