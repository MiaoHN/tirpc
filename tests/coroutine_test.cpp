#include <gtest/gtest.h>
#include "tirpc/coroutine/coroutine_pool.hpp"
#include <atomic>

using namespace tirpc;

TEST(CoroutineTest, PoolGetAndReturn) {
    CoroutinePool pool(4, 32 * 1024);
    auto cor = pool.GetCoroutineInstanse();
    ASSERT_NE(cor, nullptr);
    pool.ReturnCoroutine(cor);
}

TEST(CoroutineTest, SetCallbackAndRun) {
    CoroutinePool pool(2, 32 * 1024);
    auto cor = pool.GetCoroutineInstanse();
    std::atomic<int> value{0};
    cor->SetCallBack([&]() { value = 123; });
    Coroutine::Resume(cor.get());
    EXPECT_EQ(value, 123);
    pool.ReturnCoroutine(cor);
}

TEST(CoroutineTest, YieldResumeSwitch) {
    CoroutinePool pool(2, 32 * 1024);
    auto cor1 = pool.GetCoroutineInstanse();
    auto cor2 = pool.GetCoroutineInstanse();
    std::atomic<int> step{0};

    cor1->SetCallBack([&]() {
        step = 1;
        Coroutine::Yield();
        step = 3;
        Coroutine::Yield();
        step = 5;
    });

    cor2->SetCallBack([&]() {
        step = 2;
        Coroutine::Yield();
        step = 4;
    });

    Coroutine::Resume(cor1.get());
    EXPECT_EQ(step, 1);

    Coroutine::Resume(cor2.get());
    EXPECT_EQ(step, 2);

    Coroutine::Resume(cor1.get());
    EXPECT_EQ(step, 3);

    Coroutine::Resume(cor2.get());
    EXPECT_EQ(step, 4);

    Coroutine::Resume(cor1.get());
    EXPECT_EQ(step, 5);

    pool.ReturnCoroutine(cor1);
    pool.ReturnCoroutine(cor2);
}
