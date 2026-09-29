#include "../source/Connection.hpp"
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <memory>
#include <unordered_map>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>

using namespace server_connection;
using namespace server_eventloop;
using namespace server_socket;
using namespace server_buffer;

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

static bool WaitFor(const std::atomic<bool>& flag, int timeout_ms = 5000)
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

static uint16_t BoundPort(Socket& s)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    getsockname(s.Fd(), (struct sockaddr*)&addr, &len);
    return ntohs(addr.sin_port);
}

static std::string RecvLine(Socket& s)
{
    std::string acc;
    char buf[64];
    while (acc.find('\n') == std::string::npos)
    {
        ssize_t n = s.Recv(buf, sizeof(buf));
        if (n <= 0)
            break;
        acc.append(buf, n);
    }
    return acc;
}

// 阶段一：完整的连接生命周期 + Upgrade + Shutdown + 非活跃销毁
static void TestLifecycle(EventLoop* loop)
{
    std::cout << "[TEST] Connection lifecycle / Upgrade / Shutdown\n";

    Socket listener;
    CHECK(listener.CreateServer(0, "127.0.0.1"), "listener created");
    uint16_t port = BoundPort(listener);

    std::thread client([&]() {
        Socket c;
        if (!c.CreateClient(port, "127.0.0.1"))
            return;
        CHECK(RecvLine(c) == "welcome\n", "client recv welcome");
        CHECK(c.Send("ping\n", 5) == 5, "client send ping");
        CHECK(RecvLine(c) == "pong\n", "client recv pong");
        CHECK(c.Send("upgrade\n", 8) == 8, "client send upgrade");
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        CHECK(c.Send("data\n", 5) == 5, "client send data");
        CHECK(RecvLine(c) == "upgraded\n", "client recv upgraded");
        CHECK(c.Send("bye\n", 4) == 4, "client send bye");
        char buf[16];
        c.Recv(buf, sizeof(buf)); // 等待服务端关闭
        c.Close();
    });

    int connfd = listener.Accept();
    CHECK(connfd >= 0, "listener accept");

    const uint64_t id = 1;
    std::unordered_map<uint64_t, PtrConnection> conns;
    PtrConnection conn = std::make_shared<Connection>(loop, id, connfd);
    conns[id] = conn;

    conn->SetContext(Any(1234));
    CHECK(conn->ConnId() == id, "Connection::ConnId");
    CHECK(conn->SocketFd() == connfd, "Connection::SocketFd");
    CHECK(!conn->Connected(), "Connection::Connected false before Established");

    std::atomic<bool> connected{false}, closed{false}, server_closed{false};
    std::atomic<int> events{0}, ctx{0};

    auto event_cb = [&](const PtrConnection&) { events.fetch_add(1); };
    auto closed_cb = [&](const PtrConnection&) { closed = true; };

    auto upgraded_msg = [&](const PtrConnection& c2, ServerBuffer* b2) {
        while (b2->ReadableSize() > 0)
        {
            std::string l = b2->GetLine();
            if (l.empty())
                break;
            if (l.back() == '\n')
                l.pop_back();
            if (l == "data")
                c2->Send("upgraded\n", 9);
            else if (l == "bye")
                c2->Shutdown();
        }
    };

    auto first_msg = [&](const PtrConnection& c, ServerBuffer* buf) {
        while (buf->ReadableSize() > 0)
        {
            std::string line = buf->GetLine();
            if (line.empty())
                break;
            if (line.back() == '\n')
                line.pop_back();
            if (line == "ping")
                c->Send("pong\n", 5);
            else if (line == "upgrade")
                c->Upgrade(Any(99), [](const PtrConnection&) {}, upgraded_msg,
                           closed_cb, event_cb);
            else if (line == "bye")
                c->Shutdown();
        }
    };

    conn->SetConnectedCallback([&](const PtrConnection& c) {
        connected = true;
        CHECK(c->Connected(), "Connection::Connected true in connected cb");
        int* v = c->GetContext()->get<int>();
        if (v)
            ctx = *v;
        c->Send("welcome\n", 8);

        // 非活跃销毁：先启用定时器，再取消
        c->EnableInactiveRelease(5);
        CHECK(loop->IsTimerExist(c->ConnId()),
              "Connection::EnableInactiveRelease adds timer");
        c->CancelInactiveRelease();
    });
    conn->SetMessageCallback(first_msg);
    conn->SetClosedCallback(closed_cb);
    conn->SetAnyEventCallback(event_cb);
    conn->SetServerClosedCallBack([&](const PtrConnection& c) {
        server_closed = true;
        conns.erase(c->ConnId());
    });

    conn->Established();

    CHECK(WaitFor(server_closed), "connection closed by Shutdown");
    CHECK(connected.load(), "Connection::SetConnectedCallback invoked");
    CHECK(ctx.load() == 1234, "Connection::SetContext/GetContext");
    CHECK(closed.load(), "Connection::SetClosedCallback invoked");
    CHECK(events.load() > 0, "Connection::SetAnyEventCallback invoked");

    client.join();

    std::weak_ptr<Connection> weak = conn;
    conn.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(weak.expired(), "Connection destroyed after close");

    listener.Close();
}

// 阶段二：直接调用 Release()
static void TestRelease(EventLoop* loop)
{
    std::cout << "[TEST] Connection::Release\n";

    Socket listener;
    CHECK(listener.CreateServer(0, "127.0.0.1"), "listener created (release)");
    uint16_t port = BoundPort(listener);

    std::thread client([&]() {
        Socket c;
        if (!c.CreateClient(port, "127.0.0.1"))
            return;
        char buf[16];
        c.Recv(buf, sizeof(buf));
        c.Close();
    });

    int connfd = listener.Accept();
    CHECK(connfd >= 0, "listener accept (release)");

    const uint64_t id = 2;
    PtrConnection conn = std::make_shared<Connection>(loop, id, connfd);
    CHECK(conn->ConnId() == id, "Connection::ConnId (release)");
    CHECK(conn->SocketFd() == connfd, "Connection::SocketFd (release)");

    std::atomic<bool> closed{false}, server_closed{false};
    conn->SetClosedCallback([&](const PtrConnection&) { closed = true; });
    conn->SetServerClosedCallBack([&](const PtrConnection&) { server_closed = true; });

    std::weak_ptr<Connection> weak = conn;
    conn->Established();
    conn->Release();

    CHECK(WaitFor(server_closed), "Connection::Release triggers close");
    CHECK(closed.load(), "Closed callback invoked by Release");

    client.join();
    conn.reset();
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    CHECK(weak.expired(), "Connection destroyed after Release");

    listener.Close();
}

int main()
{
    std::cout << "===== Connection test start =====\n";

    LoopThread* lt = new LoopThread();
    EventLoop* loop = lt->GetEventLoop();

    TestLifecycle(loop);
    TestRelease(loop);

    std::cout << "===== Connection test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";
    std::exit(g_failed.load() == 0 ? 0 : 1);
    return 0;
}
