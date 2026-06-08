#pragma once

#include "EntityId.hpp"

namespace cadino::core {

struct Door {
    EntityId id{};
    EntityId host_wall{};
    double position_along{0.0};
    double width{900.0};
    double height{2100.0};
    double sill_height{0.0};
};

}  // namespace cadino::core
