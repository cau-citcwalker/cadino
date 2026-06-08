#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "command/BoxCommands.hpp"
#include "command/CommandStack.hpp"
#include "document/Document.hpp"

using namespace cadino::core;
using Catch::Matchers::WithinAbs;

namespace {

Box make_box(double x, double y, double w, double d, double h) {
    Box b;
    b.position = {x, y};
    b.size_xy = {w, d};
    b.height = h;
    return b;
}

}  // namespace

TEST_CASE("AddBoxCommand inserts and undo removes", "[command][box]") {
    Document doc;
    CommandStack stack{doc};

    auto cmd = std::make_unique<AddBoxCommand>(make_box(0, 0, 1000, 600, 750));
    stack.execute(std::move(cmd));

    REQUIRE(doc.boxes().size() == 1);
    const auto id = doc.boxes().begin()->first;
    REQUIRE(doc.find_box(id) != nullptr);

    stack.undo();
    REQUIRE(doc.boxes().empty());

    stack.redo();
    REQUIRE(doc.boxes().size() == 1);
}

TEST_CASE("ModifyBoxCommand changes dimensions and undo restores", "[command][box]") {
    Document doc;
    CommandStack stack{doc};

    stack.execute(std::make_unique<AddBoxCommand>(make_box(0, 0, 1000, 600, 750)));
    const auto id = doc.boxes().begin()->first;

    Box modified = *doc.find_box(id);
    modified.size_xy = {2000.0, 1200.0};
    modified.height = 900.0;
    stack.execute(std::make_unique<ModifyBoxCommand>(id, modified));

    REQUIRE_THAT(doc.find_box(id)->size_xy.x(), WithinAbs(2000.0, 1e-9));
    REQUIRE_THAT(doc.find_box(id)->height, WithinAbs(900.0, 1e-9));

    stack.undo();
    REQUIRE_THAT(doc.find_box(id)->size_xy.x(), WithinAbs(1000.0, 1e-9));
    REQUIRE_THAT(doc.find_box(id)->height, WithinAbs(750.0, 1e-9));
}

TEST_CASE("Document tracks walls and boxes independently in entity_count", "[document][box]") {
    Document doc;
    CommandStack stack{doc};

    Wall w;
    w.start = {0, 0};
    w.end = {1000, 0};
    doc.add_wall(w);

    stack.execute(std::make_unique<AddBoxCommand>(make_box(500, 500, 600, 600, 750)));
    REQUIRE(doc.entity_count() == 2);
    REQUIRE(doc.walls().size() == 1);
    REQUIRE(doc.boxes().size() == 1);
}
