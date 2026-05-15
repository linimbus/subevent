#include "SingleEvent.h"
#include <gtest/gtest.h>
#include <string>

struct TestEventA {
    int value;
};

struct TestEventB {
    std::string text;
};

TEST(SingleEventTest, PublishInvokesSubscriber) {
    SingleEvent bus;
    bool called = false;

    bus.Subscribe<TestEventA>([&](const TestEventA &event) {
        called = true;
        EXPECT_EQ(event.value, 42);
    });

    bus.Publish<TestEventA>({42});
    EXPECT_TRUE(called);
}

TEST(SingleEventTest, PublishInvokesAllSubscribersOfSameType) {
    SingleEvent bus;
    int callCount = 0;

    bus.Subscribe<TestEventA>([&](const TestEventA &event) {
        callCount += event.value;
    });
    bus.Subscribe<TestEventA>([&](const TestEventA &event) {
        callCount += event.value * 2;
    });

    bus.Publish<TestEventA>({5});
    EXPECT_EQ(callCount, 15);
}

TEST(SingleEventTest, DifferentEventTypesDoNotInterfere) {
    SingleEvent bus;
    bool calledA = false;
    bool calledB = false;

    bus.Subscribe<TestEventA>([&](const TestEventA &event) { calledA = true; });
    bus.Subscribe<TestEventB>([&](const TestEventB &event) {
        calledB = true;
        EXPECT_EQ(event.text, "hello");
    });

    bus.Publish<TestEventB>({"hello"});
    EXPECT_TRUE(calledB);
    EXPECT_FALSE(calledA);

    calledB = false;
    bus.Publish<TestEventA>({10});
    EXPECT_TRUE(calledA);
    EXPECT_FALSE(calledB);
}

TEST(SingleEventTest, UnsubscribeRemovesSpecificEventType) {
    SingleEvent bus;
    bool calledA = false;

    bus.Subscribe<TestEventA>([&](const TestEventA &event) { calledA = true; });
    bus.Unsubscribe<TestEventA>();
    bus.Publish<TestEventA>({100});

    EXPECT_FALSE(calledA);
}

TEST(SingleEventTest, UnsubscribeAllRemovesAllSubscribers) {
    SingleEvent bus;
    bool calledA = false;
    bool calledB = false;

    bus.Subscribe<TestEventA>([&](const TestEventA &event) { calledA = true; });
    bus.Subscribe<TestEventB>([&](const TestEventB &event) { calledB = true; });
    bus.UnsubscribeAll();

    bus.Publish<TestEventA>({1});
    bus.Publish<TestEventB>({"world"});

    EXPECT_FALSE(calledA);
    EXPECT_FALSE(calledB);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
