#include <catch/catch.hpp>
#include "../toolkit/ltl2ba.h"

TEST_CASE("basic Ltl2ba functionality", "[ltl2ba]") {
    Ltl2ba<std::string> test("G F a");
}