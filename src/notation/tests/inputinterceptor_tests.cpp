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

#include <gtest/gtest.h>

#include "context/internal/inputinterceptor.h"

using namespace mu::context;

TEST(Context_InputInterceptor, DelegatesPhaseAndKey)
{
    InputInterceptor ii;
    EXPECT_FALSE(ii.hasHandler());
    EXPECT_FALSE(ii.wouldConsume(RawKeyEvent{}));   // no handler -> false

    int seenKey = -1;
    bool seenPhase = false;
    ii.setHandler([&](const RawKeyEvent& e, bool phase) {
        seenKey = e.key;
        seenPhase = phase;
        return e.key == 74;
    });
    EXPECT_TRUE(ii.hasHandler());
    EXPECT_TRUE(ii.wouldConsume(RawKeyEvent{ 74, 0, "j", false }));
    EXPECT_EQ(seenKey, 74);
    EXPECT_TRUE(seenPhase);

    EXPECT_FALSE(ii.handleKey(RawKeyEvent{ 75, 0, "k", false }));
    EXPECT_FALSE(seenPhase); // handleKey passed phase=false

    ii.clearHandler();
    EXPECT_FALSE(ii.hasHandler());
}
