#pragma once

#include "entity/EntityId.hpp"

namespace cadino::ui {

enum class SelectKind {
    None,
    Wall,
    Box,
    Cylinder,
    NurbsCurve,
    Block,
    NurbsSurface,
};

struct Selection {
    cadino::core::EntityId id{};
    SelectKind kind{SelectKind::None};

    [[nodiscard]] bool valid() const noexcept {
        return id.valid() && kind != SelectKind::None;
    }

    constexpr bool operator==(const Selection&) const noexcept = default;
};

}  // namespace cadino::ui
