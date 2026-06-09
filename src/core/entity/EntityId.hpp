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

inline std::uint64_t& entity_id_counter() noexcept {
    static std::uint64_t counter = 0;
    return counter;
}

inline EntityId next_entity_id() noexcept {
    return EntityId{++entity_id_counter()};
}

inline void seed_entity_id_at_least(std::uint64_t value) noexcept {
    auto& c = entity_id_counter();
    if (value > c) c = value;
}

}  // namespace cadino::core

template <>
struct std::hash<cadino::core::EntityId> {
    std::size_t operator()(const cadino::core::EntityId& id) const noexcept {
        return std::hash<std::uint64_t>{}(id.value);
    }
};
