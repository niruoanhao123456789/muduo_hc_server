#include "../source/Connection.hpp"
#include <memory>

using namespace server_connection;
using namespace server_eventloop;
using namespace server_socket;
using namespace server_buffer;
using namespace LogModule;

int main()
{
    Socket client_socket;
    if(!client_socket.CreateClient(8080, "127.0.0.1"))
    {
        LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Connect 127.0.0.1:8080 failed!";
        return -1;
    }

    EventLoop loop;
    PtrConnection conn = std::make_shared<Connection>(&loop, 1, client_socket.Fd());

    conn->SetConnectedCallback([](const PtrConnection& conn){
        LOGD_STREAM() << "Client: connection established, id = " << conn->ConnId();

        std::string msg = "hello world";
        LOGD_STREAM() << "Client send: " << msg;
        conn->Send(msg.c_str(), msg.size());
    });

    conn->SetMessageCallback([](const PtrConnection& conn, ServerBuffer* buf){
        std::string msg = buf->ReadAsString(buf->ReadableSize());
        LOGD_STREAM() << "Client recv: " << msg;
        ::exit(0);
    });

    conn->SetClosedCallback([](const PtrConnection& conn){
        LOGD_STREAM() << "Client: connection closed, id = " << conn->ConnId();
    });

    conn->Established();
    loop.Start();

    return 0;
}
