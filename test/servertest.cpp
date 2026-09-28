#include "../source/Connection.hpp"
#include <memory>

using namespace server_connection;
using namespace server_eventloop;
using namespace server_socket;
using namespace server_buffer;
using namespace LogModule;

int main()
{
    Socket listen_socket;
    if(!listen_socket.CreateServer(8080, "127.0.0.1"))
    {
        LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Listen on 127.0.0.1:8080 failed!";
        return -1;
    }
    LOGD_STREAM() << "Server listening on 127.0.0.1:8080 ...";

    int connfd = listen_socket.Accept();
    if(connfd < 0)
        return -1;

    EventLoop loop;
    PtrConnection conn = std::make_shared<Connection>(&loop, 1, connfd);

    conn->SetConnectedCallback([](const PtrConnection& conn){
        LOGD_STREAM() << "Server: connection established, id = " << conn->ConnId();
    });

    conn->SetMessageCallback([](const PtrConnection& conn, ServerBuffer* buf){
        std::string msg = buf->ReadAsString(buf->ReadableSize());
        LOGD_STREAM() << "Server recv: " << msg;

        std::string reply = "hello world";
        conn->Send(reply.c_str(), reply.size());
        conn->Shutdown();
    });

    conn->SetClosedCallback([](const PtrConnection& conn){
        LOGD_STREAM() << "Server: connection closed, id = " << conn->ConnId();
        ::exit(0);
    });

    conn->Established();
    loop.Start();

    return 0;
}
