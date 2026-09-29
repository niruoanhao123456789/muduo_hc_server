#include "../source/Server.hpp"
#include "../source/Connection.hpp"
#include <memory>
#include <future>
#include <unordered_map>
#include <unistd.h>

using namespace server_connection;
using namespace server_eventloop;
using namespace server_socket;
using namespace server_channel;
using namespace server_acceptor;
using namespace server_buffer;
using namespace LogModule;

int main()
{
    // 用 LoopThread 在独立的线程中运行 EventLoop
    LoopThread loop_thread;
    EventLoop* loop = loop_thread.GetEventLoop();

    // 服务器内部管理的所有连接
    auto conns = std::make_shared<std::unordered_map<uint64_t, PtrConnection>>();
    auto next_id = std::make_shared<uint64_t>(1);

    // Acceptor 内部会操作 EventLoop 的 Poller，因此在其所属线程内创建并设置回调
    std::promise<void> inited;
    loop->RunInLoop([&]() {
        Acceptor* acceptor = new Acceptor(loop, 8080);
        acceptor->SetAcceptCallBack([loop, conns, next_id](int fd) {
            uint64_t id = (*next_id)++;
            PtrConnection conn = std::make_shared<Connection>(loop, id, fd);
            (*conns)[id] = conn;

            conn->SetConnectedCallback([](const PtrConnection& conn) {
                LOGD_STREAM() << "Server: connection established, id = " << conn->ConnId();
            });

            conn->SetMessageCallback([](const PtrConnection& conn, ServerBuffer* buf) {
                std::string msg = buf->ReadAsString(buf->ReadableSize());
                LOGD_STREAM() << "Server recv: " << msg;

                std::string reply = "hello world";
                conn->Send(reply.c_str(), reply.size());
                conn->Shutdown();
            });

            conn->SetClosedCallback([](const PtrConnection& conn) {
                LOGD_STREAM() << "Server: connection closed, id = " << conn->ConnId();
            });

            // 移除服务器内部保存的连接信息
            conn->SetServerClosedCallBack([conns](const PtrConnection& conn) {
                conns->erase(conn->ConnId());
                ::exit(0);
            });

            conn->Established();
        });
        inited.set_value();
    });
    inited.get_future().wait();

    LOGI_STREAM() << "Server listening on 127.0.0.1:8080 ...";

    // 主线程不再参与 IO，仅保持进程存活
    while (1)
        pause();

    return 0;
}
