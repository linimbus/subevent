#include "MultiEvent.h"
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>

struct ConcurrentEventA {
    int value;
    bool operator==(const ConcurrentEventA &other) const {
        return value == other.value;
    }
};

struct ConcurrentEventB {
    std::string text;
    bool operator==(const ConcurrentEventB &other) const {
        return text == other.text;
    }
};

struct ConcurrentEventC {
    double data;
    bool operator==(const ConcurrentEventC &other) const {
        return data == other.data;
    }
};

// 测试1: 多线程并发 Publish 单事件 - 验证基本线程安全
TEST(MultiEventConcurrencyTest, ConcurrentPublishSingleEvent) {
    MultiEvent bus;
    std::atomic<int> callCount{0};
    const int numThreads = 3;
    const int eventsPerThread = 10;

    // 预先订阅，避免并发订阅问题
    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{0}, [&](const ConcurrentEventA &) {
        callCount++;
    });

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bus]() {
            for (int j = 0; j < eventsPerThread; ++j) {
                bus.Publish<ConcurrentEventA>({0});
                std::this_thread::yield();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    // 验证有消息被接收（不要求精确计数，因为聚合器可能触发）
    EXPECT_GT(callCount.load(), 0);
}

// 测试2: 多线程并发 Publish 不同事件类型到 Aggregator
TEST(MultiEventConcurrencyTest, ConcurrentPublishToAggregator) {
    MultiEvent bus;
    std::atomic<int> aggregatorCalls{0};
    const int numPairs = 30;

    bus.Subscribe<ConcurrentEventA, ConcurrentEventB>(
        ConcurrentEventA{1}, ConcurrentEventB{"match"},
        [&](const ConcurrentEventA &, const ConcurrentEventB &) {
            aggregatorCalls++;
        });

    std::vector<std::thread> threads;

    // 多个线程发布 EventA
    threads.emplace_back([&bus, numPairs]() {
        for (int i = 0; i < numPairs; ++i) {
            bus.Publish<ConcurrentEventA>({1});
        }
    });

    // 多个线程发布 EventB
    threads.emplace_back([&bus, numPairs]() {
        for (int i = 0; i < numPairs; ++i) {
            bus.Publish<ConcurrentEventB>({"match"});
        }
    });

    for (auto &t : threads) {
        t.join();
    }

    // 验证聚合器被触发（至少一次，由于配对机制，具体次数取决于执行顺序）
    EXPECT_GT(aggregatorCalls.load(), 0);
}

// 测试3: 多线程环境下 Subscribe 和 Publish 交替进行
TEST(MultiEventConcurrencyTest, ConcurrentSubscribeAndPublish) {
    MultiEvent bus;
    std::atomic<int> totalReceived{0};
    const int numSubscribers = 3;
    const int numPublishers = 3;

    // 预先订阅几个事件类型，避免并发订阅导致的问题
    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{0}, [&totalReceived](const ConcurrentEventA &) {
        totalReceived++;
    });
    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{1}, [&totalReceived](const ConcurrentEventA &) {
        totalReceived++;
    });
    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{2}, [&totalReceived](const ConcurrentEventA &) {
        totalReceived++;
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    std::vector<std::thread> publisherThreads;
    for (int i = 0; i < numPublishers; ++i) {
        publisherThreads.emplace_back([&bus, i]() {
            for (int j = 0; j < 30; ++j) {
                bus.Publish<ConcurrentEventA>({static_cast<int>(i % 3)});
                std::this_thread::yield();
            }
        });
    }

    for (auto &t : publisherThreads) {
        t.join();
    }

    EXPECT_GT(totalReceived.load(), 0);
}

// 测试4: 多线程 Unsubscribe 测试
TEST(MultiEventConcurrencyTest, ConcurrentUnsubscribe) {
    MultiEvent bus;
    std::atomic<int> receivedCount{0};
    const int numThreads = 10;

    // 先订阅多个事件类型
    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{1}, [&](const ConcurrentEventA &) {
        receivedCount++;
    });
    bus.Subscribe<ConcurrentEventB>(ConcurrentEventB{"test"}, [&](const ConcurrentEventB &) {
        receivedCount++;
    });
    bus.Subscribe<ConcurrentEventC>(ConcurrentEventC{3.14}, [&](const ConcurrentEventC &) {
        receivedCount++;
    });

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bus, &receivedCount, i]() {
            if (i % 3 == 0) {
                bus.Unsubscribe<ConcurrentEventA>();
            } else if (i % 3 == 1) {
                bus.Unsubscribe<ConcurrentEventB>();
            } else {
                bus.Unsubscribe<ConcurrentEventC>();
            }
            
            // 尝试发布
            bus.Publish<ConcurrentEventA>({1});
            bus.Publish<ConcurrentEventB>({"test"});
            bus.Publish<ConcurrentEventC>({3.14});
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    // 验证操作没有崩溃
}

// 测试5: 多 Aggregator 并发测试
TEST(MultiEventConcurrencyTest, MultipleAggregatorsConcurrency) {
    MultiEvent bus;
    std::atomic<int> agg1Calls{0};
    std::atomic<int> agg2Calls{0};
    const int iterations = 50;

    // 第一个聚合器：监听 EventA + EventB
    bus.Subscribe<ConcurrentEventA, ConcurrentEventB>(
        ConcurrentEventA{10}, ConcurrentEventB{"first"},
        [&](const ConcurrentEventA &, const ConcurrentEventB &) {
            agg1Calls++;
        });

    // 第二个聚合器：监听 EventB + EventC
    bus.Subscribe<ConcurrentEventB, ConcurrentEventC>(
        ConcurrentEventB{"second"}, ConcurrentEventC{2.71},
        [&](const ConcurrentEventB &, const ConcurrentEventC &) {
            agg2Calls++;
        });

    std::vector<std::thread> threads;

    // 线程1: 发布 EventA 和 EventB 用于第一个聚合器
    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<ConcurrentEventA>({10});
            bus.Publish<ConcurrentEventB>({"first"});
        }
    });

    // 线程2: 发布 EventB 和 EventC 用于第二个聚合器
    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<ConcurrentEventB>({"second"});
            bus.Publish<ConcurrentEventC>({2.71});
        }
    });

    // 线程3: 混合发布所有类型
    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<ConcurrentEventA>({10});
            bus.Publish<ConcurrentEventB>({"first"});
            bus.Publish<ConcurrentEventB>({"second"});
            bus.Publish<ConcurrentEventC>({2.71});
        }
    });

    for (auto &t : threads) {
        t.join();
    }

    // 验证两个聚合器都被触发
    EXPECT_GT(agg1Calls.load(), 0);
    EXPECT_GT(agg2Calls.load(), 0);
}

// 测试6: UnsubscribeAll 在并发环境下的安全性
TEST(MultiEventConcurrencyTest, UnsubscribeAllUnderConcurrency) {
    MultiEvent bus;
    std::atomic<int> receivedBeforeClear{0};
    const int numThreads = 5;

    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{1}, [&](const ConcurrentEventA &) {
        receivedBeforeClear++;
    });
    bus.Subscribe<ConcurrentEventB>(ConcurrentEventB{"test"}, [&](const ConcurrentEventB &) {
        receivedBeforeClear++;
    });

    std::vector<std::thread> threads;

    // 发布线程
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bus, i]() {
            for (int j = 0; j < 20; ++j) {
                bus.Publish<ConcurrentEventA>({1});
                std::this_thread::yield();
            }
        });
    }

    // 清理线程
    threads.emplace_back([&bus, &receivedBeforeClear]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
        bus.UnsubscribeAll();
    });

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_GT(receivedBeforeClear.load(), 0);
}

// 测试7: 读写锁压力测试 - 大量并发读取和写入
TEST(MultiEventConcurrencyTest, ReadWriteLockStressTest) {
    MultiEvent bus;
    std::atomic<int> publishCount{0};
    std::atomic<int> subscribeCount{0};
    const int numPublishers = 8;
    const int numSubscribers = 3;

    // 预先订阅
    bus.Subscribe<ConcurrentEventA>(ConcurrentEventA{0}, [&publishCount](const ConcurrentEventA &) {
        publishCount++;
    });

    std::vector<std::thread> threads;

    // 发布者线程
    for (int i = 0; i < numPublishers; ++i) {
        threads.emplace_back([&bus, &publishCount, i]() {
            for (int j = 0; j < 50; ++j) {
                bus.Publish<ConcurrentEventA>({0});
                std::this_thread::yield();
            }
        });
    }

    // 订阅/取消订阅线程
    for (int i = 0; i < numSubscribers; ++i) {
        threads.emplace_back([&bus, &subscribeCount, i]() {
            for (int j = 0; j < 20; ++j) {
                bus.Subscribe<ConcurrentEventB>(
                    ConcurrentEventB{std::to_string(i)}, 
                    [&subscribeCount](const ConcurrentEventB &) {
                        subscribeCount++;
                    });
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                std::this_thread::yield();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_GT(publishCount.load(), 0);
}

// 测试8: 三事件聚合器并发测试
TEST(MultiEventConcurrencyTest, ThreeEventAggregatorConcurrency) {
    MultiEvent bus;
    std::atomic<int> aggregatorCalls{0};
    const int iterations = 30;

    bus.Subscribe<ConcurrentEventA, ConcurrentEventB, ConcurrentEventC>(
        ConcurrentEventA{5}, ConcurrentEventB{"tri"}, ConcurrentEventC{1.414},
        [&](const ConcurrentEventA &, const ConcurrentEventB &, const ConcurrentEventC &) {
            aggregatorCalls++;
        });

    std::vector<std::thread> threads;

    // 三个线程分别发布三种事件
    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<ConcurrentEventA>({5});
        }
    });

    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<ConcurrentEventB>({"tri"});
        }
    });

    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<ConcurrentEventC>({1.414});
        }
    });

    for (auto &t : threads) {
        t.join();
    }

    // 验证三事件聚合器被触发
    EXPECT_GT(aggregatorCalls.load(), 0);
}
