#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "entity/Wall.hpp"

using namespace cadino::core;
using Catch::Matchers::WithinAbs;

TEST_CASE("Wall length is the distance between start and end", "[wall]") {
    Wall w;
    w.start = {0.0, 0.0};
    w.end = {3000.0, 4000.0};

    REQUIRE_THAT(w.length(), WithinAbs(5000.0, 1e-6));
}

TEST_CASE("Wall direction is unit vector from start to end", "[wall]") {
    Wall w;
    w.start = {0.0, 0.0};
    w.end = {10.0, 0.0};

    const auto d = w.direction();
    REQUIRE_THAT(d.x(), WithinAbs(1.0, 1e-9));
    REQUIRE_THAT(d.y(), WithinAbs(0.0, 1e-9));
}

TEST_CASE("Zero-length wall returns a safe direction", "[wall]") {
    Wall w;
    w.start = {1.0, 1.0};
    w.end = {1.0, 1.0};

    const auto d = w.direction();
    REQUIRE_THAT(d.norm(), WithinAbs(1.0, 1e-9));
}

TEST_CASE("Wall normal is perpendicular to direction", "[wall]") {
    Wall w;
    w.start = {0.0, 0.0};
    w.end = {10.0, 0.0};

    const auto d = w.direction();
    const auto n = w.normal();
    REQUIRE_THAT(d.dot(n), WithinAbs(0.0, 1e-9));
}
