#include <catch/catch.hpp>
#include "../toolkit/ltl2ba.h"

TEST_CASE("basic Ltl2ba functionality", "[ltl2ba]") {
    Ltl2ba<LtlTranslator> test("G F (1 + 2 = 3 => 2 + 1 = 8)");
    auto g = test.get_ba();
}