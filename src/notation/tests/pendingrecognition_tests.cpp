/* SPDX-License-Identifier: GPL-3.0-only */
#include <gtest/gtest.h>
#include "notation/internal/pendingrecognition.h"

using namespace mu::notation;

TEST(Notation_PendingRecognition, AddFindClear)
{
    PendingRegistry reg;
    EXPECT_EQ(reg.size(), 0);

    // Use opaque non-null pointers as identity keys (no deref in the registry).
    const auto* elA = reinterpret_cast<const mu::engraving::EngravingItem*>(0x1000);
    const auto* elB = reinterpret_cast<const mu::engraving::EngravingItem*>(0x2000);

    PendingRecognition p;
    p.element = elA;
    p.recognized.label = "Quarter-Note";
    reg.add(p);

    EXPECT_EQ(reg.size(), 1);
    ASSERT_TRUE(reg.find(elA).has_value());
    EXPECT_EQ(reg.find(elA)->recognized.label, QString("Quarter-Note"));
    EXPECT_FALSE(reg.find(elB).has_value());
    EXPECT_FALSE(reg.find(nullptr).has_value());

    reg.clear();
    EXPECT_EQ(reg.size(), 0);
    EXPECT_FALSE(reg.find(elA).has_value());
}
