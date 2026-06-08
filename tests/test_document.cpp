#include <catch2/catch_test_macros.hpp>

#include "document/Document.hpp"

using namespace cadino::core;

TEST_CASE("Document starts empty", "[document]") {
    Document doc;
    REQUIRE(doc.entity_count() == 0);
}

TEST_CASE("Adding a wall assigns an id and is retrievable", "[document]") {
    Document doc;
    Wall w;
    w.start = {0.0, 0.0};
    w.end = {1000.0, 0.0};

    const auto id = doc.add_wall(w);
    REQUIRE(id.valid());
    REQUIRE(doc.entity_count() == 1);

    const auto* found = doc.find_wall(id);
    REQUIRE(found != nullptr);
    REQUIRE(found->id == id);
}

TEST_CASE("Removing a wall reduces the count", "[document]") {
    Document doc;
    Wall w;
    const auto id = doc.add_wall(w);

    REQUIRE(doc.remove_wall(id));
    REQUIRE(doc.entity_count() == 0);
    REQUIRE(doc.find_wall(id) == nullptr);
}

TEST_CASE("Door is hosted by a wall and added separately", "[document]") {
    Document doc;
    Wall w;
    w.start = {0.0, 0.0};
    w.end = {5000.0, 0.0};
    const auto wall_id = doc.add_wall(w);

    Door d;
    d.host_wall = wall_id;
    d.position_along = 2000.0;
    const auto door_id = doc.add_door(d);

    REQUIRE(door_id.valid());
    REQUIRE(door_id != wall_id);
    REQUIRE(doc.entity_count() == 2);

    const auto* found = doc.find_door(door_id);
    REQUIRE(found != nullptr);
    REQUIRE(found->host_wall == wall_id);
}

TEST_CASE("Window and Slab can also be added", "[document]") {
    Document doc;
    Window win;
    Slab slab;
    slab.outline = {{0,0}, {5000,0}, {5000,3000}, {0,3000}};

    const auto win_id = doc.add_window(win);
    const auto slab_id = doc.add_slab(slab);

    REQUIRE(doc.find_window(win_id) != nullptr);
    REQUIRE(doc.find_slab(slab_id) != nullptr);
    REQUIRE(doc.entity_count() == 2);
}
