#pragma once

#include <cstdint>
#include <functional>

namespace cadino::core {

struct EntityId {
    std::uint64_t value{0};

    constexpr bool operator==(const EntityId&) const noexcept = default;
    constexpr auto operator<=>(const EntityId&) const noexcept = default;

    [[nodiscard]] constexpr bool valid() const noexcept { return value != 0; }
};

inline EntityId next_entity_id() noexcept {
    static std::uint64_t counter = 0;
    return EntityId{++counter};
}

}  // namespace cadino::core

template <>
struct std::hash<cadino::core::EntityId> {
    std::size_t operator()(const cadino::core::EntityId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
