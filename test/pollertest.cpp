#include "../source/EventLoop.hpp"
#include "../source/Poller.hpp"
#include <atomic>
#include <thread>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <unistd.h>

using namespace server_eventloop;
using namespace server_poller;
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

int main()
{
    std::cout << "===== Poller test start =====\n";

    int pfd[2];
    if (pipe(pfd) != 0)
    {
        std::cout << "pipe failed\n";
        return 1;
    }

    // 借助一个未启动的 EventLoop 来设置 Channel 的事件位；真正测试的是独立的 Poller
    EventLoop loop;
    Poller poller;
    Channel ch(&loop, pfd[0]);

    ch.EnableRead(); // 设置 _events 为 EPOLLIN（同时注册到 loop 的 Poller）
    poller.UpdateEvent(&ch); // Poller::UpdateEvent -> EPOLL_CTL_ADD

    // 第二次 UpdateEvent 走 EPOLL_CTL_MOD 分支
    poller.UpdateEvent(&ch);

    std::vector<Channel*> active;
    std::thread poller_thread([&]() { poller.Poll(&active); });

    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    const char c = 'a';
    CHECK(::write(pfd[1], &c, 1) == 1, "pipe write");

    poller_thread.join();

    CHECK(active.size() == 1, "Poller::Poll returns one active channel");
    CHECK(!active.empty() && active[0] == &ch,
          "Poller::Poll returns the registered channel");
    CHECK(ch.Readable(), "Poller::Poll keeps Channel event flags");

    // 清空管道里的数据
    char b;
    CHECK(::read(pfd[0], &b, 1) == 1, "pipe read back");

    // Poller::RemoveEvent
    poller.RemoveEvent(&ch);

    close(pfd[0]);
    close(pfd[1]);

    std::cout << "===== Poller test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";
    return g_failed.load() == 0 ? 0 : 1;
}
