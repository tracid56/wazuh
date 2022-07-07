#include <vector>

#include <gtest/gtest.h>

#include <baseTypes.hpp>

#include "combinatorBuilderChain.hpp"
#include "opBuilderCondition.hpp"
#include "opBuilderHelperFilter.hpp"
#include "opBuilderHelperMap.hpp"
#include "opBuilderMap.hpp"
#include "opBuilderMapValue.hpp"
#include "stageBuilderCheck.hpp"
#include "stageBuilderNormalize.hpp"
#include "testUtils.hpp"

using namespace base;
namespace bld = builder::internals::builders;

using FakeTrFn = std::function<void(std::string)>;
static FakeTrFn tr = [](std::string msg) {
};

class opBuilderHelperStringContainment : public testing::Test
{
protected:
    // Per-test-suite set-up.
    // Called before the first test in this test suite.
    static void SetUpTestSuite()
    {
        Registry::registerBuilder("check", bld::stageBuilderCheck);
        Registry::registerBuilder("condition", bld::opBuilderCondition);
        Registry::registerBuilder("map", bld::opBuilderMap);
        Registry::registerBuilder("map.value", bld::opBuilderMapValue);
        Registry::registerBuilder("middle.condition", bld::middleBuilderCondition);
        Registry::registerBuilder("middle.helper.exists", bld::opBuilderHelperExists);
        Registry::registerBuilder("middle.helper.s_contains",
                                  bld::opBuilderHelperStringContains);
        Registry::registerBuilder("middle.helper.s_not_contains",
                                  bld::opBuilderHelperStringNotContains);
        Registry::registerBuilder("combinator.chain", bld::combinatorBuilderChain);
    }

    // Per-test-suite tear-down.
    // Called after the last test in this test suite.
    static void TearDownTestSuite() { return; }
};

TEST_F(opBuilderHelperStringContainment, Builds)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_contains/First/Second"}
    })"};

    ASSERT_NO_THROW(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
}

TEST_F(opBuilderHelperStringContainment, WrongNumberOfArguments)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_contains"}
    })"};

    ASSERT_THROW(bld::opBuilderHelperStringContains(doc.get("/check"), tr),
                 std::runtime_error);
}

TEST_F(opBuilderHelperStringContainment, EmptyArguments)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_contains//"}
    })"};

    ASSERT_THROW(bld::opBuilderHelperStringContains(doc.get("/check"), tr),
                 std::runtime_error);
}

TEST_F(opBuilderHelperStringContainment, BasicUsageOk)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_contains/First"}
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Firstly"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Seccond"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Firs"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "SeccondFirst"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
}

TEST_F(opBuilderHelperStringContainment, SimpleWithOneReference)
{
    Document doc {R"({
        "check":
        {
            "fieldResult": "+s_contains/$fieldToCompare"
        }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Variable",
                "fieldToCompare": "Var"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Var",
                "fieldToCompare": "Variable"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Variable",
                "fieldToCompare": "NOT"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Var",
                "fieldToCompare": "Var"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
}

TEST_F(opBuilderHelperStringContainment, DoubleWithReferences)
{
    Document doc {R"({
        "check":
        {
            "fieldResult": "+s_contains/$fieldToCompare/$anotherField"
        }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "ABC",
                "fieldToCompare": "A",
                "anotherField": "B"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "ABC",
                "fieldToCompare": "Var",
                "anotherField": "B"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "ABC",
                "fieldToCompare": "C",
                "anotherField": "A"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "ABC",
                "fieldToCompare": "NOT",
                "anotherField": "8"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "ABC",
                "fieldToCompare": "ABC",
                "anotherField": "EFGH"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
}

TEST_F(opBuilderHelperStringContainment, OneReferencesNotString)
{
    Document doc {R"({
        "check":
        {
            "Field": "+s_contains/$fieldToCompare"
        }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"Field": "ABCDEFG",
                "fieldToCompare": 1}
            )"));
        s.on_next(createSharedEvent(R"(
                {"Field": "123456",
                "fieldToCompare": 1}
            )"));
        s.on_next(createSharedEvent(R"(
                {"Field": "123456",
                "fieldToCompare": "1"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"Field": "ABCD-EFG",
                "fieldToCompare": null}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
}

TEST_F(opBuilderHelperStringContainment, OneEmptyReference)
{
    Document doc {R"({
        "check":
        {
            "Field": "+s_contains/$fieldToCompare/$anotherField"
        }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"anotherField": "",
                "Field": "Value",
                "fieldToCompare": "V"}
            )"));
        s.on_completed();
    });
    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 0);
}

TEST_F(opBuilderHelperStringContainment, ReferenceDoesntExist)
{
    Document doc {R"({
        "check":
        {
            "Field": "+s_contains/$anotherField"
        }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"Field": "something",
                "fieldToCompare": "s"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 0);
}

TEST_F(opBuilderHelperStringContainment, DoubleUsageOk)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_contains/First/Seccond"}
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "First-Seccond"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Seccond-First"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "FirsSec"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Seccond"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Seccond"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Seccond123456798First123456789"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 3);
}

TEST_F(opBuilderHelperStringContainment, SeveralFieldsUsageOk)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_contains/A/B/C/D/E/F/G"}
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "A"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "AB"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "ABC"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "ABCD"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "ABCDE"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "ABCDEF"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "ABCDEFG"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "ABCDEFGH"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 2);
}

TEST_F(opBuilderHelperStringContainment, BasicUsageThreeArgumentsMiddleEmpty)
{
    Document doc {R"({
            "check":
            {
                "Field": "+s_contains/First//Third"
            }
    })"};

    ASSERT_THROW(bld::opBuilderHelperStringContains(doc.get("/check"), tr),
                 std::runtime_error);
}

TEST_F(opBuilderHelperStringContainment, DoubleUsage)
{
    Document doc {R"({
        "normalize":
        [
            {
                "check":
                [
                    { "FieldB": "+s_contains/$FieldA/D/E/F/G"}
                ],
                "map":
                {
                    "FieldX": "A"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"FieldA": "ABC",
                "FieldB": "ABCDEFG"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"FieldA": "Z",
                "FieldB": "ABCDEFG"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"FieldA": "",
                "FieldB": "ABCDEFG"}
            )"));
        s.on_completed();
    });

    Lifter lift = bld::stageBuilderNormalize(doc.get("/normalize"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
}

TEST_F(opBuilderHelperStringContainment, AssignmentOnNestedField)
{
    Document doc {R"({
            "check":
            {
                "parent1.fieldToCompare":
                "+s_contains/$parent2.fieldToCompare"
            }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                    "parent1" : {"fieldToCompare": "ABC"},
                    "parent2" : {"fieldToCompare": "DEF"}
                }
            )"));
        s.on_next(createSharedEvent(R"(
                {
                    "parent1" : {"fieldToCompare": "1"},
                    "parent2" : {"fieldToCompare": "DEF"}
                }
            )"));
        s.on_next(createSharedEvent(R"(
                {
                    "parent1" : {"fieldToCompare": "ABC"},
                    "parent2" : {"fieldToCompare": "A"}
                }
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
}

// NOT CONTAINS //

TEST_F(opBuilderHelperStringContainment, NotContainsBasicUsageOk)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_not_contains/First"}
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Firstly"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "SeccondFirst"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "Firs"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "SeccondFirst"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringNotContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
}

TEST_F(opBuilderHelperStringContainment, NotContainsSimpleWithOneReference)
{
    Document doc {R"({
        "check":
        {
            "fieldResult": "+s_not_contains/$fieldToCompare"
        }
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Variable",
                "fieldToCompare": "Var"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Var",
                "fieldToCompare": "Variable"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Variable",
                "fieldToCompare": "NOT"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldResult": "Var",
                "fieldToCompare": "Other"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringNotContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 3);
}

TEST_F(opBuilderHelperStringContainment, NotContainsSeveralFieldsUsageOk)
{
    Document doc {R"({
        "check":
            {"fieldToCompare": "+s_not_contains/A/B/C/D/E"}
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1ABCDE"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1ABCD"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1ABC"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1AB"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1A"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1A"}
            )"));
        s.on_next(createSharedEvent(R"(
                {"fieldToCompare": "1"}
            )"));
        s.on_completed();
    });

    Lifter lift = [=](Observable input) {
        return input.filter(bld::opBuilderHelperStringNotContains(doc.get("/check"), tr));
    };
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
}
