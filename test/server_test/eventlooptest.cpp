#include "../../source/Server.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <future>
#include <iostream>
#include <cstdlib>

using namespace server_eventloop;

static std::atomic<int> g_passed{0};
static std::atomic<int> g_failed{0};

#define CHECK(cond, msg)                                                       \
    do {                                                                       \
        if (cond) {                                                            \
            g_passed.fetch_add(1);                                             \
        } else {                                                               \
            g_failed.fetch_add(1);                                             \
            std::cout << "[FAIL] " << (msg) << "  (" << __FILE__ << ":"        \
                      << __LINE__ << ")\n";                                    \
        }                                                                      \
    } while (0)

// 轮询等待标志位，超时返回 false
static bool WaitFor(const std::atomic<bool>& flag, int timeout_ms = 3000)
{
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (!flag.load())
    {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

// 把函数投递到 EventLoop 线程执行，并同步等待其完成
template <class F>
static void RunSync(EventLoop* loop, F&& f)
{
    std::promise<void> done;
    auto fut = done.get_future();
    loop->RunInLoop([&f, &done]() {
        f();
        done.set_value();
    });
    fut.wait();
}

static void TestLoopThread()
{
    std::cout << "[TEST] LoopThread\n";

    // LoopThread 在独立线程中创建并运行 EventLoop；应使用 new/不析构，
    // 因为 EventLoop::Start() 是常驻循环，进程退出前线程不会结束。
    LoopThread* lt = new LoopThread();
    EventLoop* loop = lt->GetEventLoop();

    CHECK(loop != nullptr, "LoopThread::GetEventLoop returns non-null");
    CHECK(!loop->IsInLoop(),
          "EventLoop::IsInLoop false when called from foreign thread");

    // RunInLoop：从外部线程调用应投递到 loop 线程执行
    std::atomic<bool> ran_in_loop{false};
    std::thread::id caller_tid = std::this_thread::get_id();
    std::thread::id loop_tid;
    RunSync(loop, [&]() {
        loop_tid = std::this_thread::get_id();
        loop->AssertInLoop(); // 在 loop 线程内不应触发断言
        CHECK(loop->IsInLoop(), "EventLoop::IsInLoop true inside loop thread");
        ran_in_loop = true;
    });
    CHECK(ran_in_loop.load(), "EventLoop::RunInLoop executes task");
    CHECK(loop_tid != caller_tid,
          "EventLoop::RunInLoop runs task on loop thread");

    // RunInLoop：在 loop 线程内调用应立即执行
    bool direct = false, observed = false;
    RunSync(loop, [&]() {
        loop->RunInLoop([&]() { direct = true; });
        observed = direct; // 若在同线程，direct 立刻为 true
    });
    CHECK(observed,
          "EventLoop::RunInLoop executes directly inside loop thread");

    // PushQueueInLoop：从外部线程入队，并靠 eventfd 唤醒阻塞的 epoll
    std::atomic<bool> pushed{false};
    loop->PushQueueInLoop([&]() { pushed = true; });
    CHECK(WaitFor(pushed), "EventLoop::PushQueueInLoop task executed (wakeup)");

    // PushQueueInLoop：从另一个工作线程入队
    std::atomic<bool> pushed2{false};
    std::thread worker([&]() {
        loop->PushQueueInLoop([&]() { pushed2 = true; });
    });
    worker.join();
    CHECK(WaitFor(pushed2),
          "EventLoop::PushQueueInLoop from worker thread executed");

    // RunAllTask：应一次性执行已入队的全部任务
    int cnt = 0;
    RunSync(loop, [&]() {
        loop->PushQueueInLoop([&]() { ++cnt; });
        loop->PushQueueInLoop([&]() { ++cnt; });
        loop->RunAllTask();
    });
    CHECK(cnt == 2, "EventLoop::RunAllTask drains queued tasks");

    // 子线程内的任务均复用该 loop，进程退出前保持存活
    (void)loop;
}

static void TestTimer()
{
    std::cout << "[TEST] EventLoop::TimerAdd/TimerRefresh/TimerCancel/IsTimerExist\n";

    LoopThread* lt = new LoopThread();
    EventLoop* loop = lt->GetEventLoop();

    // ---- TimerAdd + IsTimerExist + 到期回调 ----
    {
        std::atomic<bool> fired{false};
        const uint64_t id = 1001;
        loop->TimerAdd(id, 2, [&]() { fired = true; });

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        bool exists = false;
        RunSync(loop, [&]() { exists = loop->IsTimerExist(id); });
        CHECK(exists, "EventLoop::IsTimerExist true after TimerAdd");

        CHECK(WaitFor(fired, 5000), "EventLoop::TimerAdd callback fired");

        std::this_thread::sleep_for(std::chrono::milliseconds(300));
        bool exists_after = true;
        RunSync(loop, [&]() { exists_after = loop->IsTimerExist(id); });
        CHECK(!exists_after, "EventLoop::IsTimerExist false after fired");
    }

    // ---- TimerRefresh：刷新后应推迟到期 ----
    {
        std::atomic<bool> fired{false};
        const uint64_t id = 1002;
        loop->TimerAdd(id, 2, [&]() { fired = true; });

        // 在第一次到期前刷新，把到期时间向后推
        std::this_thread::sleep_for(std::chrono::milliseconds(1500));
        loop->TimerRefresh(id);

        // 原定到期点之后应立即确认尚未触发
        std::this_thread::sleep_for(std::chrono::milliseconds(900));
        CHECK(!fired.load(), "EventLoop::TimerRefresh delays timeout");

        CHECK(WaitFor(fired, 4000), "TimerRefresh callback eventually fired");
    }

    // ---- TimerCancel：取消后不应触发 ----
    {
        std::atomic<bool> fired{false};
        const uint64_t id = 1003;
        loop->TimerAdd(id, 1, [&]() { fired = true; });
        loop->TimerCancel(id);

        std::this_thread::sleep_for(std::chrono::seconds(3));
        CHECK(!fired.load(), "EventLoop::TimerCancel prevents callback");
    }

    (void)loop;
}

static void TestEventLoopEventReg()
{
    std::cout << "[TEST] EventLoop::UpdateEvent/RemoveEvent\n";

    LoopThread* lt = new LoopThread();
    EventLoop* loop = lt->GetEventLoop();

    int pfd[2];
    if (pipe(pfd) != 0)
    {
        CHECK(false, "pipe create failed");
        return;
    }

    // UpdateEvent / RemoveEvent 直接操作 Poller，需在 loop 线程内进行
    RunSync(loop, [&]() {
        Channel ch(loop, pfd[0]);
        ch.SetReadCallBack([&]() { char b; ssize_t n = ::read(pfd[0], &b, 1); (void)n; });
        loop->UpdateEvent(&ch); // 添加
        loop->UpdateEvent(&ch); // 已存在 -> 修改
        loop->RemoveEvent(&ch); // 移除
        CHECK(true, "EventLoop::UpdateEvent/RemoveEvent no crash");
    });

    close(pfd[0]);
    close(pfd[1]);
    (void)loop;
}

int main()
{
    std::cout << "===== EventLoop / LoopThread test start =====\n";

    TestLoopThread();
    TestTimer();
    TestEventLoopEventReg();

    std::cout << "===== EventLoop / LoopThread test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";

    // LoopThread 内的线程常驻，使用 exit 跳过其析构
    std::exit(g_failed.load() == 0 ? 0 : 1);
    return 0;
}
