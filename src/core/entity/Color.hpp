#pragma once

namespace cadino::core {

struct Color {
    float r{0.78f};
    float g{0.78f};
    float b{0.80f};

    constexpr bool operator==(const Color&) const noexcept = default;
};

}  // namespace cadino::core
