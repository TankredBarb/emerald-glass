// SPDX-License-Identifier: LGPL-3.0-or-later
#pragma once

#include <algorithm>

namespace EmeraldGlass {

struct BlurProfile {
    int iterations;
    float offset;
    int expandSize;
};

// The fifteen Dual Kawase steps shared by the effect and the settings preview.
inline BlurProfile blurProfile(int strength)
{
    strength = std::clamp(strength, 0, 15);
    if (strength == 0) return {0, 0.0f, 0};
    if (strength <= 2) return {1, 1.0f + strength * 0.5f, 10};
    if (strength <= 4) return {2, 2.0f + (strength - 2) * 0.5f, 20};
    if (strength <= 9) return {3, 2.0f + (strength - 4) * 0.6f, 50};
    return {4, 3.0f + (strength - 9) * (5.0f / 6.0f), 150};
}

} // namespace EmeraldGlass
