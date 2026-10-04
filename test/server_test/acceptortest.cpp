#include "../../source/Server.hpp"
#include <atomic>
#include <future>
#include <string>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <thread>
#include <chrono>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace server_acceptor;
using namespace server_eventloop;
using namespace server_socket;

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

static uint16_t FindFreePort()
{
    Socket s;
    s.Create();
    s.Bind("127.0.0.1", 0);
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    getsockname(s.Fd(), (struct sockaddr*)&addr, &len);
    uint16_t port = ntohs(addr.sin_port);
    s.Close();
    return port;
}

int main()
{
    std::cout << "===== Acceptor test start =====\n";

    LoopThread* lt = new LoopThread();
    EventLoop* loop = lt->GetEventLoop();

    uint16_t port = FindFreePort();
    CHECK(port != 0, "found free port");

    std::atomic<int> accepted_fd{-1};
    std::atomic<bool> echoed{false};

    Acceptor* acceptor = nullptr;
    std::promise<void> inited;
    loop->RunInLoop([&]() {
        acceptor = new Acceptor(loop, port);
        acceptor->SetAcceptCallBack([&](int fd) {
            accepted_fd = fd;
            Socket conn(fd);
            char buf[64] = {0};
            ssize_t n = conn.Recv(buf, sizeof(buf));
            if (n > 0)
                conn.Send(buf, n);
            echoed = true;
            conn.Close();
        });
        inited.set_value();
    });
    inited.get_future().wait();
    CHECK(acceptor != nullptr, "Acceptor created on loop thread");

    Socket client;
    if (client.CreateClient(port, "127.0.0.1"))
    {
        CHECK(client.Send("hello", 5) == 5, "client send");

        char rbuf[64] = {0};
        ssize_t rn = client.Recv(rbuf, sizeof(rbuf));
        CHECK(rn == 5 && std::string(rbuf, 5) == "hello",
              "Acceptor callback received/sent data");
        client.Close();
    }
    else
    {
        CHECK(false, "client connect to acceptor");
    }

    // 等待回调执行完成
    for (int i = 0; i < 200 && !echoed.load(); ++i)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));

    CHECK(accepted_fd.load() >= 0, "accept callback invoked");

    std::cout << "===== Acceptor test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";
    std::exit(g_failed.load() == 0 ? 0 : 1);
    return 0;
}
