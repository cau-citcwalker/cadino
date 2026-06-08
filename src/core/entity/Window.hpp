#pragma once

#include "EntityId.hpp"

namespace cadino::core {

struct Window {
    EntityId id{};
    EntityId host_wall{};
    double position_along{0.0};
    double width{1200.0};
    double height{1500.0};
    double sill_height{900.0};
};

}  // namespace cadino::core
