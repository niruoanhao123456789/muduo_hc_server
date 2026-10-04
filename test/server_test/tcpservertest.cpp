#include "../../source/Server.hpp"
#include <atomic>
#include <future>
#include <string>
#include <thread>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdlib>
#include <cstring>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

using namespace server_tcp;
using namespace server_eventloop;
using namespace server_socket;
using namespace server_connection;
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

static bool WaitForCount(const std::atomic<int>& counter, int target,
                         int timeout_ms = 5000)
{
    auto deadline = std::chrono::steady_clock::now() +
                    std::chrono::milliseconds(timeout_ms);
    while (counter.load() < target)
    {
        if (std::chrono::steady_clock::now() > deadline)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

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

static void SetRecvTimeout(Socket& s, int sec)
{
    struct timeval tv;
    tv.tv_sec = sec;
    tv.tv_usec = 0;
    setsockopt(s.Fd(), SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
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

// 从服务端消息回调中解析一行命令
static void HandleEchoLine(const PtrConnection& conn, ServerBuffer* buf,
                           std::atomic<int>* counter)
{
    while (buf->ReadableSize() > 0)
    {
        std::string line = buf->GetLine();
        if (line.empty())
            break;
        if (line.back() == '\n')
            line.pop_back();
        if (counter)
            counter->fetch_add(1);
        if (line == "ping" || line == "hi")
            conn->Send("pong\n", 5);
        else if (line == "bye")
            conn->Shutdown();
    }
}

// 阶段一：完整生命周期 + 全部回调 + 线程池 + RunAfter
static void TestEchoLifecycle()
{
    std::cout << "[TEST] TcpServer lifecycle / callbacks / thread pool / RunAfter\n";

    uint16_t port = FindFreePort();
    CHECK(port != 0, "found free port (lifecycle)");

    std::atomic<bool> started{false}, connected{false};
    std::atomic<bool> closed{false}, any_event{false};
    std::atomic<int> msg_count{0};
    std::atomic<TcpServer*> server_ptr{nullptr};
    std::promise<void> ready;

    std::thread srv([&]() {
        TcpServer* server = new TcpServer(port);
        server->SetThreadCount(2);
        server->SetConnectedCallBack([&](const PtrConnection& c) {
            connected = true;
            CHECK(c->Connected(), "connection CONNECTED in connected callback");
        });
        server->SetMessageCallBack([&](const PtrConnection& c, ServerBuffer* buf) {
            HandleEchoLine(c, buf, &msg_count);
        });
        server->SetClosedCallBack([&](const PtrConnection&) { closed = true; });
        server->SetAnyEventCallBack([&](const PtrConnection&) { any_event = true; });

        server_ptr = server;
        started = true;
        ready.set_value();
        server->Start(); // 常驻循环，阻塞该线程
    });

    ready.get_future().wait();
    CHECK(started.load(), "TcpServer constructed with thread pool and started");
    TcpServer* server = server_ptr.load();
    CHECK(server != nullptr, "TcpServer instance available");

    Socket client;
    CHECK(client.CreateClient(port, "127.0.0.1"), "client connect to TcpServer");
    SetRecvTimeout(client, 2);

    CHECK(client.Send("ping\n", 5) == 5, "client send ping");
    CHECK(RecvLine(client) == "pong\n", "client recv pong from TcpServer");

    CHECK(WaitFor(connected), "TcpServer::SetConnectedCallBack invoked");
    CHECK(WaitFor(any_event), "TcpServer::SetAnyEventCallBack invoked");

    CHECK(client.Send("bye\n", 4) == 4, "client send bye");
    char tmp[16];
    client.Recv(tmp, sizeof(tmp)); // 等待服务端关闭
    client.Close();

    CHECK(WaitFor(closed), "TcpServer::SetClosedCallBack invoked");
    CHECK(msg_count.load() >= 2, "TcpServer::SetMessageCallBack handled ping & bye");

    // RunAfter：跨线程投递一个 1 秒定时任务
    std::atomic<bool> after_ran{false};
    server->RunAfter([&]() { after_ran = true; }, 1);
    CHECK(WaitFor(after_ran, 4000), "TcpServer::RunAfter executes task");

    srv.detach(); // Start() 常驻，进程退出时一并结束
}

// 阶段二：不设置线程池，连接应回落到 base loop
static void TestSingleThreadEcho()
{
    std::cout << "[TEST] TcpServer default single-thread echo\n";

    uint16_t port = FindFreePort();
    CHECK(port != 0, "found free port (single)");

    std::atomic<bool> connected{false};
    std::atomic<int> msg_count{0};
    std::promise<void> ready;

    std::thread srv([&]() {
        TcpServer* server = new TcpServer(port);
        server->SetConnectedCallBack([&](const PtrConnection&) { connected = true; });
        server->SetMessageCallBack([&](const PtrConnection& c, ServerBuffer* buf) {
            HandleEchoLine(c, buf, &msg_count);
        });
        ready.set_value();
        server->Start();
    });

    ready.get_future().wait();

    Socket client;
    CHECK(client.CreateClient(port, "127.0.0.1"), "client connect (single thread)");
    SetRecvTimeout(client, 2);

    CHECK(client.Send("ping\n", 5) == 5, "client send ping (single thread)");
    CHECK(RecvLine(client) == "pong\n", "single-thread server echoes");
    CHECK(WaitFor(connected), "single-thread connected callback invoked");
    (void)msg_count;

    client.Close();
    srv.detach();
}

// 阶段三：启用非活跃连接超时销毁
static void TestInactiveRelease()
{
    std::cout << "[TEST] TcpServer::EnableInactiveRelease\n";

    uint16_t port = FindFreePort();
    CHECK(port != 0, "found free port (inactive)");

    std::atomic<bool> closed{false};
    std::promise<void> ready;

    std::thread srv([&]() {
        TcpServer* server = new TcpServer(port);
        server->EnableInactiveRelease(1); // 1 秒无通信即释放
        server->SetMessageCallBack([&](const PtrConnection&, ServerBuffer*) {});
        server->SetClosedCallBack([&](const PtrConnection&) { closed = true; });
        ready.set_value();
        server->Start();
    });

    ready.get_future().wait();

    Socket client;
    CHECK(client.CreateClient(port, "127.0.0.1"), "client connect (inactive)");

    // 客户端保持连接但不发数据，服务端应在超时后主动释放
    CHECK(WaitFor(closed, 6000), "TcpServer releases inactive connection");
    client.Close();
    srv.detach();
}

// 阶段四：多客户端并发，验证线程池连接分配
static void TestMultiClient()
{
    std::cout << "[TEST] TcpServer thread pool with multiple clients\n";

    uint16_t port = FindFreePort();
    CHECK(port != 0, "found free port (multi)");

    std::atomic<int> connected{0};
    std::promise<void> ready;

    std::thread srv([&]() {
        TcpServer* server = new TcpServer(port);
        server->SetThreadCount(3);
        server->SetConnectedCallBack([&](const PtrConnection&) {
            connected.fetch_add(1);
        });
        server->SetMessageCallBack([&](const PtrConnection& c, ServerBuffer* buf) {
            HandleEchoLine(c, buf, nullptr);
        });
        ready.set_value();
        server->Start();
    });

    ready.get_future().wait();

    const int kClients = 3;
    std::atomic<int> ok{0};
    std::vector<std::thread> clients;
    for (int i = 0; i < kClients; i++)
    {
        clients.emplace_back([&]() {
            Socket c;
            if (!c.CreateClient(port, "127.0.0.1"))
                return;
            SetRecvTimeout(c, 2);
            if (c.Send("hi\n", 3) == 3 && RecvLine(c) == "pong\n")
                ok.fetch_add(1);
            c.Close();
        });
    }
    for (auto& t : clients)
        t.join();

    CHECK(WaitForCount(connected, kClients),
          "all clients connected through thread pool");
    CHECK(ok.load() == kClients, "all clients echoed by thread pool");

    srv.detach();
}

int main()
{
    std::cout << "===== TcpServer test start =====\n";

    TestEchoLifecycle();
    TestSingleThreadEcho();
    TestInactiveRelease();
    TestMultiClient();

    std::cout << "===== TcpServer test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";

    // TcpServer::Start() 常驻，各服务线程不会退出，用 exit 跳过析构
    std::exit(g_failed.load() == 0 ? 0 : 1);
    return 0;
}
