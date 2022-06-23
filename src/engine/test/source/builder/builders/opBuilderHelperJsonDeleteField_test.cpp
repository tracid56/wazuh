#include <vector>

#include <gtest/gtest.h>

#include <baseTypes.hpp>

#include "combinatorBuilderChain.hpp"
#include "opBuilderCondition.hpp"
#include "opBuilderHelperFilter.hpp"
#include "opBuilderHelperMap.hpp"
#include "stageBuilderCheck.hpp"
#include "stageBuilderNormalize.hpp"
#include "testUtils.hpp"

using namespace base;
namespace bld = builder::internals::builders;

using FakeTrFn = std::function<void(std::string)>;
static FakeTrFn tr = [](std::string msg) {
};

class opBuilderHelperJsonDeleteFieldsTestSuite : public testing::Test
{
protected:
    // Per-test-suite set-up.
    // Called before the first test in this test suite.
    static void SetUpTestSuite()
    {

        // Registry::registerBuilder("check", bld::stageBuilderCheck);
        // Registry::registerBuilder("condition", bld::opBuilderCondition);
        // Registry::registerBuilder("middle.condition", bld::middleBuilderCondition);
        // Registry::registerBuilder("middle.helper.exists", bld::opBuilderHelperExists);
        // Registry::registerBuilder("combinator.chain", bld::combinatorBuilderChain);
        // Registry::registerBuilder("helper.s_concat", bld::opBuilderHelperStringConcat);
    }

    // Per-test-suite tear-down.
    // Called after the last test in this test suite.
    static void TearDownTestSuite() { return; }
};

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, Builds)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "qttyOfDeletedFields": "+json_delete_fields/First/Second"
                }
            }
        ]
    })"};

    ASSERT_NO_THROW(
        bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr));
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, CantBuildWithoutParameter)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "qttyOfDeletedFields": "+json_delete_fields"
                }
            }
        ]
    })"};

    ASSERT_THROW(bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr),
                 std::runtime_error);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, CantBuildWithEmptyParameter)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "qttyOfDeletedFields": "+json_delete_fields//"
                }
            }
        ]
    })"};

    ASSERT_THROW(bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr),
                 std::runtime_error);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, CantBuildWithoutStringParameter)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "qttyOfDeletedFields": "+json_delete_fields//2"
                }
            }
        ]
    })"};

    ASSERT_THROW(bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr),
                 std::runtime_error);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, ExecutesWithTwoDeletes)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/First/Second"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": "1",
                "Second": "2",
                "Third":"whatever"
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 2);
    ASSERT_THROW(expected[0]->getEventValue("/First"), std::invalid_argument);
    ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, ExecutesWithReference)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/$First/Second"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": "Third",
                "Second": "2",
                "Third":""
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 2);
    ASSERT_EQ(expected[0]->getEventValue("/First"), "Third");
    ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
    ASSERT_THROW(expected[0]->getEventValue("/Third"), std::invalid_argument);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, CantExecuteNonStringReference)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/$First/Second"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": 8,
                "Second": "2",
                "Third": 9
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/First"), 8);
    ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
    ASSERT_EQ(expected[0]->getEventValue("/Third").GetInt(), 9);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, DeleteJustFirstReference)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/$First/$Second"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": "Second",
                "Second": "Second",
                "Third": 9
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 1);
    ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, CantDeleteUnexistentField)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/Second"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": "something"
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 0);
    ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, CantDeleteUnexistentField_2)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/Second/First"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": "something"
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    ASSERT_EQ(expected.size(), 1);
    ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 1);
    ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
    ASSERT_THROW(expected[0]->getEventValue("/First"), std::invalid_argument);
}

TEST_F(opBuilderHelperJsonDeleteFieldsTestSuite, DeleteJustNestedReference)
{
    Document doc {R"({
        "normalize":
        [
            {
                "map":
                {
                    "deletedFields": "+json_delete_fields/Second.a"
                }
            }
        ]
    })"};

    Observable input = observable<>::create<Event>([=](auto s) {
        s.on_next(createSharedEvent(R"(
                {
                "First": "anotherThing",
                "Second":
                    { "a" : "something" },
                "Third": 9
                }
            )"));
        s.on_completed();
    });

    Lifter lift = bld::opBuilderHelperJsonDeleteFields(doc.get("/normalize/0/map"), tr);
    Observable output = lift(input);
    vector<Event> expected;
    output.subscribe([&](Event e) { expected.push_back(e); });
    std::cout << expected[0]->getEvent()->prettyStr() << std::endl;
    // ASSERT_EQ(expected.size(), 1);
    // ASSERT_EQ(expected[0]->getEventValue("/deletedFields").GetInt(), 1);
    // ASSERT_THROW(expected[0]->getEventValue("/Second"), std::invalid_argument);
}

//Test with array