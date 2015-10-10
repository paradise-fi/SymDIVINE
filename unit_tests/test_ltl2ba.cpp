#include <catch/catch.hpp>
#include "../llvmsym/programutils/config.h"
#include "../toolkit/ltl2ba.h"

TEST_CASE("basic Ltl2ba functionality", "[ltl2ba]") {
    Config.set("--verbose", true);
    Config.set("--vverbose", true);
	Ltl2ba<LtlTranslator> test("G F (1 + 2 = 3 => 2 + 1 = 8)");
	auto g = test.get_ba();
}