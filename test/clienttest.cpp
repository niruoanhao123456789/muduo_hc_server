#include "../source/Connection.hpp"
#include <memory>
#include <future>
#include <unistd.h>

using namespace server_connection;
using namespace server_eventloop;
using namespace server_socket;
using namespace server_buffer;
using namespace LogModule;

int main()
{
    // 客户端同样使用 LoopThread，在独立线程中运行 EventLoop
    LoopThread loop_thread;
    EventLoop* loop = loop_thread.GetEventLoop();

    Socket client_socket;
    if(!client_socket.CreateClient(8080, "127.0.0.1"))
    {
        LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Connect 127.0.0.1:8080 failed!";
        return -1;
    }

    PtrConnection conn = std::make_shared<Connection>(loop, 1, client_socket.Fd());

    conn->SetConnectedCallback([](const PtrConnection& conn){
        LOGD_STREAM() << "Client: connection established, id = " << conn->ConnId();

        std::string msg = "hello world";
        LOGD_STREAM() << "Client send: " << msg;
        conn->Send(msg.c_str(), msg.size());
    });

    conn->SetMessageCallback([](const PtrConnection& conn, ServerBuffer* buf){
        std::string msg = buf->ReadAsString(buf->ReadableSize());
        LOGD_STREAM() << "Client recv: " << msg;
        conn->Shutdown();
    });

    conn->SetClosedCallback([](const PtrConnection& conn){
        LOGD_STREAM() << "Client: connection closed, id = " << conn->ConnId();
        ::exit(0);
    });

    conn->Established();

    while (1)
        pause();

    return 0;
}
