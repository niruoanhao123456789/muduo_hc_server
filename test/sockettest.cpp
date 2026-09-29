#include "../source/Socket.hpp"
#include <atomic>
#include <thread>
#include <chrono>
#include <string>
#include <iostream>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

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

static uint16_t BoundPort(Socket& s)
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    socklen_t len = sizeof(addr);
    getsockname(s.Fd(), (struct sockaddr*)&addr, &len);
    return ntohs(addr.sin_port);
}

static void TestBasicAcceptConnect()
{
    std::cout << "[TEST] Socket basic (Create/Bind/Listen/Accept/Connect/Recv/Send)\n";

    Socket server;
    CHECK(server.Fd() == -1, "Socket default Fd is -1");
    CHECK(server.Create(), "Socket::Create");
    CHECK(server.Fd() >= 0, "Socket::Create assigns fd");

    server.ReuseAddress();
    int opt = 0;
    socklen_t olen = sizeof(opt);
    getsockopt(server.Fd(), SOL_SOCKET, SO_REUSEADDR, &opt, &olen);
    CHECK(opt != 0, "Socket::ReuseAddress sets SO_REUSEADDR");

    CHECK(server.Bind("127.0.0.1", 0), "Socket::Bind");
    CHECK(server.Listen(), "Socket::Listen");
    uint16_t port = BoundPort(server);
    CHECK(port != 0, "Socket bound to an ephemeral port");

    std::atomic<int> accepted_fd{-1};
    std::thread st([&]() {
        int afd = server.Accept();
        accepted_fd = afd;
        if (afd < 0)
            return;
        Socket conn(afd);
        char buf[64] = {0};
        ssize_t n = conn.Recv(buf, sizeof(buf));
        if (n > 0)
            conn.Send(buf, n);
        conn.Close();
    });

    Socket client;
    CHECK(client.Create(), "Socket::Create (client)");
    CHECK(client.Connect("127.0.0.1", port), "Socket::Connect");
    CHECK(client.Send("ping", 4) == 4, "Socket::Send");

    char rbuf[64] = {0};
    ssize_t rn = client.Recv(rbuf, sizeof(rbuf));
    CHECK(rn == 4 && std::string(rbuf, 4) == "ping", "Socket::Recv");

    st.join();
    CHECK(accepted_fd.load() >= 0, "Socket::Accept returns a connection");

    client.Close();
    CHECK(client.Fd() == -1, "Socket::Close resets fd");
    server.Close();
    CHECK(server.Fd() == -1, "Socket::Close resets listener fd");
}

static void TestCreateServerClientAndNonBlock()
{
    std::cout << "[TEST] Socket (CreateServer/CreateClient/NonBlockRecv/NonBlockSend/NonBlock)\n";

    Socket sserver;
    CHECK(sserver.CreateServer(0, "127.0.0.1", false), "Socket::CreateServer");
    uint16_t port = BoundPort(sserver);
    CHECK(port != 0, "CreateServer bound to an ephemeral port");

    std::thread st([&]() {
        int afd = sserver.Accept();
        if (afd < 0)
            return;
        Socket conn(afd);
        char buf[64] = {0};
        ssize_t n = 0;
        while ((n = conn.NonBlockRecv(buf, sizeof(buf))) == 0)
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        if (n > 0)
            conn.NonBlockSend(buf, n);
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        conn.Close();
    });

    Socket sclient;
    CHECK(sclient.CreateClient(port, "127.0.0.1"), "Socket::CreateClient");
    CHECK(sclient.Send("data", 4) == 4, "Socket::Send via CreateServer path");

    char rbuf[64] = {0};
    ssize_t rn = 0;
    while ((rn = sclient.NonBlockRecv(rbuf, sizeof(rbuf))) == 0)
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    CHECK(rn == 4 && std::string(rbuf, 4) == "data",
          "Socket::NonBlockRecv/NonBlockSend");

    st.join();
    sclient.Close();
    sserver.Close();

    // NonBlock 标志位
    Socket ns;
    CHECK(ns.Create(), "Socket::Create (nonblock)");
    ns.NonBlock();
    int fl = fcntl(ns.Fd(), F_GETFL, 0);
    CHECK((fl & O_NONBLOCK) != 0, "Socket::NonBlock sets O_NONBLOCK");
    ns.Close();
}

int main()
{
    std::cout << "===== Socket test start =====\n";
    TestBasicAcceptConnect();
    TestCreateServerClientAndNonBlock();
    std::cout << "===== Socket test end =====\n";
    std::cout << "passed: " << g_passed.load()
              << ", failed: " << g_failed.load() << "\n";
    return g_failed.load() == 0 ? 0 : 1;
}
