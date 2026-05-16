#include "SingleEvent.h"
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>
#include <chrono>
#include <mutex>

struct ConcurrentEvent {
    int value;
};

struct StressEvent {
    std::string data;
};

// 测试1: 多线程并发 Publish
TEST(SingleEventConcurrencyTest, ConcurrentPublishFromMultipleThreads) {
    SingleEvent bus;
    std::atomic<int> callCount{0};
    const int numThreads = 10;
    const int eventsPerThread = 100;

    bus.Subscribe<ConcurrentEvent>([&](const ConcurrentEvent &event) {
        callCount++;
    });

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bus, i, eventsPerThread]() {
            for (int j = 0; j < eventsPerThread; ++j) {
                bus.Publish<ConcurrentEvent>({i * 1000 + j});
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    EXPECT_EQ(callCount.load(), numThreads * eventsPerThread);
}

// 测试2: 多线程并发 Subscribe 和 Publish
TEST(SingleEventConcurrencyTest, ConcurrentSubscribeAndPublish) {
    SingleEvent bus;
    std::atomic<int> totalReceived{0};
    std::atomic<bool> stopFlag{false};
    const int numPublishers = 5;
    const int numSubscribers = 5;

    // 启动多个订阅者线程，动态添加订阅
    std::vector<std::thread> subscriberThreads;
    for (int i = 0; i < numSubscribers; ++i) {
        subscriberThreads.emplace_back([&bus, &totalReceived, i]() {
            bus.Subscribe<StressEvent>([&totalReceived, i](const StressEvent &event) {
                totalReceived++;
            });
            // 订阅后等待一小段时间
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        });
    }

    // 等待订阅完成
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 启动多个发布者线程
    std::vector<std::thread> publisherThreads;
    for (int i = 0; i < numPublishers; ++i) {
        publisherThreads.emplace_back([&bus, i]() {
            for (int j = 0; j < 50; ++j) {
                bus.Publish<StressEvent>({std::to_string(i * 100 + j)});
            }
        });
    }

    // 等待所有订阅者线程完成
    for (auto &t : subscriberThreads) {
        t.join();
    }

    // 等待所有发布者线程完成
    for (auto &t : publisherThreads) {
        t.join();
    }

    // 验证至少有一些消息被接收（由于订阅时机问题，不要求全部）
    EXPECT_GT(totalReceived.load(), 0);
}

// 测试3: 多线程环境下 Subscribe 和 Unsubscribe 交替进行
TEST(SingleEventConcurrencyTest, ConcurrentSubscribeAndUnsubscribe) {
    SingleEvent bus;
    std::atomic<int> successCount{0};
    const int numThreads = 10;

    std::vector<std::thread> threads;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bus, &successCount, i]() {
            if (i % 2 == 0) {
                // 偶数线程：订阅并发布
                bus.Subscribe<ConcurrentEvent>([&successCount](const ConcurrentEvent &event) {
                    successCount++;
                });
                bus.Publish<ConcurrentEvent>({i});
            } else {
                // 奇数线程：取消订阅
                bus.Unsubscribe<ConcurrentEvent>();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    // 验证操作没有崩溃，最终状态一致
    // 由于并发执行，具体调用次数不确定，但不应崩溃
}

// 测试4: 读写锁线程安全测试 - 大量并发读取和写入
TEST(SingleEventConcurrencyTest, ReadWriteLockStressTest) {
    SingleEvent bus;
    std::atomic<int> readCount{0};
    std::atomic<int> writeCount{0};
    const int numReaders = 4;
    const int numWriters = 2;

    // 先订阅一个事件
    bus.Subscribe<ConcurrentEvent>([&readCount](const ConcurrentEvent &) {
        readCount++;
    });

    std::vector<std::thread> threads;

    // 创建读者线程（Publish）
    for (int i = 0; i < numReaders; ++i) {
        threads.emplace_back([&bus, &readCount, i]() {
            for (int j = 0; j < 50; ++j) {
                bus.Publish<ConcurrentEvent>({i * 100 + j});
                std::this_thread::yield();
            }
        });
    }

    // 创建写者线程（Subscribe only）
    for (int i = 0; i < numWriters; ++i) {
        threads.emplace_back([&bus, &writeCount, i]() {
            for (int j = 0; j < 20; ++j) {
                bus.Subscribe<StressEvent>([&writeCount](const StressEvent &) {
                    writeCount++;
                });
                std::this_thread::sleep_for(std::chrono::microseconds(10));
                std::this_thread::yield();
            }
        });
    }

    for (auto &t : threads) {
        t.join();
    }

    // 验证读者线程成功执行
    EXPECT_GT(readCount.load(), 0);
    // 验证写者线程成功执行
    EXPECT_GT(writeCount.load(), 0);
}

// 测试5: UnsubscribeAll 在并发环境下的安全性
TEST(SingleEventConcurrencyTest, UnsubscribeAllUnderConcurrency) {
    SingleEvent bus;
    std::atomic<int> receivedBeforeClear{0};
    std::atomic<int> receivedAfterClear{0};
    const int numThreads = 5;

    // 订阅事件
    bus.Subscribe<ConcurrentEvent>([&receivedBeforeClear](const ConcurrentEvent &event) {
        receivedBeforeClear++;
    });

    std::vector<std::thread> threads;

    // 一些线程发布事件
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back([&bus, &receivedBeforeClear, i]() {
            for (int j = 0; j < 20; ++j) {
                bus.Publish<ConcurrentEvent>({i * 20 + j});
            }
        });
    }

    // 一个线程执行 UnsubscribeAll
    threads.emplace_back([&bus, &receivedAfterClear]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        bus.UnsubscribeAll();
        
        // 清理后尝试发布，应该不会被接收
        bus.Publish<ConcurrentEvent>({999});
    });

    for (auto &t : threads) {
        t.join();
    }

    // 验证在 UnsubscribeAll 之前有消息被接收
    EXPECT_GT(receivedBeforeClear.load(), 0);
}

// 测试6: 多线程下不同事件类型的隔离性
TEST(SingleEventConcurrencyTest, ThreadSafeEventTypeIsolation) {
    SingleEvent bus;
    std::atomic<int> countA{0};
    std::atomic<int> countB{0};
    std::atomic<int> countC{0};
    const int iterations = 100;

    struct EventA { int value; };
    struct EventB { int value; };
    struct EventC { int value; };

    bus.Subscribe<EventA>([&countA](const EventA &) { countA++; });
    bus.Subscribe<EventB>([&countB](const EventB &) { countB++; });
    bus.Subscribe<EventC>([&countC](const EventC &) { countC++; });

    std::vector<std::thread> threads;

    // 每个线程发布不同类型的事件
    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<EventA>({i});
        }
    });

    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<EventB>({i});
        }
    });

    threads.emplace_back([&bus, iterations]() {
        for (int i = 0; i < iterations; ++i) {
            bus.Publish<EventC>({i});
        }
    });

    for (auto &t : threads) {
        t.join();
    }

    // 验证每种类型的事件都被正确处理，且互不干扰
    EXPECT_EQ(countA.load(), iterations);
    EXPECT_EQ(countB.load(), iterations);
    EXPECT_EQ(countC.load(), iterations);
}
