#include "MultiEvent.h"
#include <gtest/gtest.h>
#include <string>

struct TestEventA {
    int value;
    bool operator==(const TestEventA &other) const {
        return value == other.value;
    }
};

struct TestEventB {
    std::string text;
    bool operator==(const TestEventB &other) const {
        return text == other.text;
    }
};

TEST(MultiEventTest, PublishSingleSubscriber) {
    MultiEvent bus;
    bool called = false;

    bus.Subscribe<TestEventA>(TestEventA{7}, [&](const TestEventA &event) {
        called = true;
        EXPECT_EQ(event.value, 7);
    });

    bus.Publish<TestEventA>({7});
    EXPECT_TRUE(called);
}

TEST(MultiEventTest, MultipleSubscribersOfSameTypeAreInvoked) {
    MultiEvent bus;
    int total = 0;

    bus.Subscribe<TestEventA>(TestEventA{1}, [&](const TestEventA &event) { total += event.value; });
    bus.Subscribe<TestEventA>(TestEventA{3}, [&](const TestEventA &event) { total += event.value * 2; });

    bus.Publish<TestEventA>({3});
    bus.Publish<TestEventA>({4});

    EXPECT_EQ(total, 6);
}

TEST(MultiEventTest, AggregatorFiresOnlyAfterAllConditionsMatch) {
    MultiEvent bus;
    bool called = false;

    bus.Subscribe<TestEventA, TestEventB>(TestEventA{1}, TestEventB{"ok"},
                                          [&](const TestEventA &a, const TestEventB &b) {
                                              called = true;
                                              EXPECT_EQ(a.value, 1);
                                              EXPECT_EQ(b.text, "ok");
                                          });

    bus.Publish<TestEventA>({1});
    EXPECT_FALSE(called);

    bus.Publish<TestEventB>({"ok"});
    EXPECT_TRUE(called);
}

TEST(MultiEventTest, AggregatorDoesNotFireForNonMatchingCondition) {
    MultiEvent bus;
    bool called = false;

    bus.Subscribe<TestEventA, TestEventB>(TestEventA{1}, TestEventB{"ok"},
                                          [&](const TestEventA &, const TestEventB &) { called = true; });

    bus.Publish<TestEventA>({2});
    bus.Publish<TestEventB>({"ok"});
    EXPECT_FALSE(called);
}

TEST(MultiEventTest, UnsubscribeRemovesSingleEventType) {
    MultiEvent bus;
    bool called = false;

    bus.Subscribe<TestEventA>(TestEventA{5}, [&](const TestEventA &) { called = true; });
    bus.Unsubscribe<TestEventA>();
    bus.Publish<TestEventA>({5});

    EXPECT_FALSE(called);
}

TEST(MultiEventTest, UnsubscribeAllClearsSubscribers) {
    MultiEvent bus;
    bool calledA = false;
    bool calledB = false;

    bus.Subscribe<TestEventA>(TestEventA{1}, [&](const TestEventA &) { calledA = true; });
    bus.Subscribe<TestEventB>(TestEventB{"hello"}, [&](const TestEventB &) { calledB = true; });
    bus.UnsubscribeAll();

    bus.Publish<TestEventA>({1});
    bus.Publish<TestEventB>({"hello"});

    EXPECT_FALSE(calledA);
    EXPECT_FALSE(calledB);
}
