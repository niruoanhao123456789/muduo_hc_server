#pragma once
#include "../source/Server.hpp"

using namespace server_tcp; 
using namespace LogModule;

class EchoServer
{
public:
    EchoServer(int port)
    :_server(port)
    {
        _server.SetThreadCount(3);
        _server.EnableInactiveRelease(10);
        _server.SetClosedCallBack(std::bind(&EchoServer::OnClosed,this,std::placeholders::_1));
        _server.SetConnectedCallBack(std::bind(&EchoServer::OnConnected,this,std::placeholders::_1));
        _server.SetMessageCallBack(std::bind(&EchoServer::OnMessage,this,std::placeholders::_1,std::placeholders::_2));
    }

    void Start()
    {
        _server.Start();
    }

private:
    void OnConnected(const PtrConnection& conn)
    {
        LOGD("new connection:%p",conn.get());
    }

    void OnClosed(const PtrConnection& conn)
    {
        LOGD("close connection:%p",conn.get());
    }

    void OnMessage(const PtrConnection& conn, ServerBuffer* buf)
    {
        buf->MoveRindex(buf->ReadableSize());
        std::string s = "Hello world";
        conn->Send(s.c_str(),s.size());
        conn->Shutdown();
    }

private:
    TcpServer _server;
};