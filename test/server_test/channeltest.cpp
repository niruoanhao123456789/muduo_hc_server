#include "../../source/Server.hpp"
#include <atomic>
#include <chrono>
#include <thread>
#include <future>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <sys/epoll.h>

using namespace server_eventloop;
using namespace server_channel;

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

// 直接调用 HandleEvent，验证事件分发、SetREvents 以及各回调 setter
static void TestDispatchAndState(EventLoop* loop, int fd)
{
    RunSync(loop, [&]() {
        Channel ch(loop, fd);

        CHECK(ch.Fd() == fd, "Channel::Fd");
        CHECK(ch.Events() == 0, "Channel::Events initial is 0");
        CHECK(!ch.Readable(), "Channel::Readable initial false");
        CHECK(!ch.Writeable(), "Channel::Writeable initial false");

        std::atomic<int> read_cb{0}, write_cb{0}, err_cb{0}, close_cb{0}, any_cb{0};

        ch.SetReadCallBack([&]() { read_cb.fetch_add(1); });
        ch.SetWriteCallBack([&]() { write_cb.fetch_add(1); });
        ch.SetErrorCallBack([&]() { err_cb.fetch_add(1); });
        ch.SetCloseCallBack([&]() { close_cb.fetch_add(1); });
        ch.SetEventCallBack([&]() { any_cb.fetch_add(1); });

        ch.SetREvents(EPOLLIN);
        ch.HandleEvent();
        CHECK(read_cb.load() == 1, "Channel::HandleEvent dispatches EPOLLIN");

        ch.SetREvents(EPOLLOUT);
        ch.HandleEvent();
        CHECK(write_cb.load() == 1, "Channel::HandleEvent dispatches EPOLLOUT");

        ch.SetREvents(EPOLLERR);
        ch.HandleEvent();
        CHECK(err_cb.load() == 1, "Channel::HandleEvent dispatches EPOLLERR");

        ch.SetREvents(EPOLLHUP);
        ch.HandleEvent();
        CHECK(close_cb.load() == 1, "Channel::HandleEvent dispatches EPOLLHUP");

        ch.SetREvents(0);
        ch.HandleEvent();
        CHECK(any_cb.load() == 5, "Channel::HandleEvent always calls event cb");

        ch.SetREvents(EPOLLRDHUP);
        ch.HandleEvent();
        CHECK(read_cb.load() == 2, "Channel::HandleEvent treats EPOLLRDHUP as read");

        // 事件监控的启停（内部会调用 Channel::Update -> EventLoop::UpdateEvent）
        ch.EnableRead();
        CHECK(ch.Readable(), "Channel::EnableRead sets readable");
        ch.EnableWrite();
        CHECK(ch.Writeable(), "Channel::EnableWrite sets writeable");
        ch.DisableRead();
        CHECK(!ch.Readable(), "Channel::DisableRead clears readable");
        ch.DisableWrite();
        CHECK(!ch.Writeable(), "Channel::DisableWrite clears writeable");
        ch.DisableAll();
        CHECK(ch.Events() == 0, "Channel::DisableAll clears events");

        ch.Update(); // 显式 Update
        ch.Remove(); // 从 Poller 移除
    });
}

// 借助真实 epoll 事件验证 Poller::Poll + HandleEvent
static void TestRealEvent(EventLoop* loop, int rfd, int wfd)
{
    std::atomic<bool> got_read{false};
    std::atomic<bool> got_event{false};

    Channel* ch = new Channel(loop, rfd);
    RunSync(loop, [&]() {
        ch->SetReadCallBack([&]() {
            char b;
            ssize_t n = ::read(rfd, &b, 1);
            if (n == 1)
                got_read = true;
        });
        ch->SetEventCallBack([&]() { got_event = true; });
        ch->EnableRead();
    });

    const char c = 'x';
    CHECK(::write(wfd, &c, 1) == 1, "pipe write for epoll");
    CHECK(WaitFor(got_read), "Channel read callback triggered by epoll");
    CHECK(WaitFor(got_event), "Channel event callback triggered by epoll");

    RunSync(loop, [&]() { ch->Remove(); });
    delete ch;
}

int main()
{
    std::cout << "===== Channel test start =====\n";

    LoopThread* lt = new LoopThread();
    EventLoop* loop = lt->GetEventLoop();

    int pfd[2];
    if (pipe(pfd) != 0)
    {
        std::cout << "pipe failed\n";
        return 1;
    }

    TestDispatchAndState(loop, pfd[0]);
    TestRealEvent(loop, pfd[0], pfd[1]);

    close(pfd[0]);
    close(pfd[1]);

    std::cout << "===== Channel test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";
    std::exit(g_failed.load() == 0 ? 0 : 1);
    return 0;
}
