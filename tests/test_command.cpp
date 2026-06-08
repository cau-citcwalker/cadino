#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "command/CommandStack.hpp"
#include "command/WallCommands.hpp"
#include "document/Document.hpp"

using namespace cadino::core;
using Catch::Matchers::WithinAbs;

namespace {

Wall make_wall(double x1, double y1, double x2, double y2) {
    Wall w;
    w.start = {x1, y1};
    w.end = {x2, y2};
    return w;
}

}  // namespace

TEST_CASE("AddWallCommand adds and undo removes", "[command][wall]") {
    Document doc;
    CommandStack stack{doc};

    auto cmd = std::make_unique<AddWallCommand>(make_wall(0, 0, 1000, 0));
    auto* cmd_ptr = cmd.get();
    stack.execute(std::move(cmd));

    REQUIRE(doc.entity_count() == 1);
    REQUIRE(cmd_ptr->entity_id().valid());
    REQUIRE(stack.can_undo());
    REQUIRE_FALSE(stack.can_redo());

    stack.undo();
    REQUIRE(doc.entity_count() == 0);
    REQUIRE_FALSE(stack.can_undo());
    REQUIRE(stack.can_redo());

    stack.redo();
    REQUIRE(doc.entity_count() == 1);
    REQUIRE(doc.find_wall(cmd_ptr->entity_id()) != nullptr);
}

TEST_CASE("ModifyWallCommand changes properties, undo restores", "[command][wall]") {
    Document doc;
    CommandStack stack{doc};

    auto add = std::make_unique<AddWallCommand>(make_wall(0, 0, 1000, 0));
    const auto id = add->entity_id();  // not assigned yet
    stack.execute(std::move(add));
    const auto wall_id = doc.walls().begin()->first;

    Wall modified = *doc.find_wall(wall_id);
    modified.end = {2000.0, 0.0};
    stack.execute(std::make_unique<ModifyWallCommand>(wall_id, modified));

    REQUIRE_THAT(doc.find_wall(wall_id)->length(), WithinAbs(2000.0, 1e-9));

    stack.undo();
    REQUIRE_THAT(doc.find_wall(wall_id)->length(), WithinAbs(1000.0, 1e-9));

    stack.redo();
    REQUIRE_THAT(doc.find_wall(wall_id)->length(), WithinAbs(2000.0, 1e-9));
    (void)id;
}

TEST_CASE("RemoveWallCommand removes, undo restores with same id", "[command][wall]") {
    Document doc;
    CommandStack stack{doc};

    stack.execute(std::make_unique<AddWallCommand>(make_wall(0, 0, 1000, 0)));
    const auto wall_id = doc.walls().begin()->first;

    stack.execute(std::make_unique<RemoveWallCommand>(wall_id));
    REQUIRE(doc.entity_count() == 0);

    stack.undo();
    REQUIRE(doc.entity_count() == 1);
    REQUIRE(doc.find_wall(wall_id) != nullptr);
}

TEST_CASE("New command after undo clears redo stack", "[command]") {
    Document doc;
    CommandStack stack{doc};

    stack.execute(std::make_unique<AddWallCommand>(make_wall(0, 0, 1000, 0)));
    stack.execute(std::make_unique<AddWallCommand>(make_wall(0, 0, 0, 1000)));
    stack.undo();

    REQUIRE(stack.can_redo());
    stack.execute(std::make_unique<AddWallCommand>(make_wall(0, 0, 500, 500)));
    REQUIRE_FALSE(stack.can_redo());
    REQUIRE(doc.entity_count() == 2);
}

TEST_CASE("Undo with empty stack is a no-op", "[command]") {
    Document doc;
    CommandStack stack{doc};

    REQUIRE_FALSE(stack.can_undo());
    stack.undo();
    REQUIRE(doc.entity_count() == 0);
}
