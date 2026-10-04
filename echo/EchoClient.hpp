#pragma once
#include "../source/Server.hpp"
#include <cstdlib>

using namespace server_tcp; 
using namespace LogModule;

class EchoClient
{
public:
    EchoClient(const std::string& ip, uint16_t port)
    :_ip(ip)
    ,_port(port)
    {}

    void Start()
    {
        Socket socket;
        if(!socket.CreateClient(_port, _ip))
        {
            LOGD("connect %s:%d failed!", _ip.c_str(), _port);
            exit(1);
        }

        EventLoop* loop = _loop_thread.GetEventLoop();
        _conn = std::make_shared<Connection>(loop, 1, socket.Fd());
        _conn->SetConnectedCallback(std::bind(&EchoClient::OnConnected,this,std::placeholders::_1));
        _conn->SetMessageCallback(std::bind(&EchoClient::OnMessage,this,std::placeholders::_1,std::placeholders::_2));
        _conn->SetClosedCallback(std::bind(&EchoClient::OnClosed,this,std::placeholders::_1));
        _conn->Established();

        while(1) pause();
    }

private:
    void OnConnected(const PtrConnection& conn)
    {
        LOGD("new connection:%p",conn.get());
        std::string s = "Hello world";
        LOGD("client send:%s",s.c_str());
        conn->Send(s.c_str(),s.size());
    }

    void OnMessage(const PtrConnection& conn, ServerBuffer* buf)
    {
        std::string s = buf->ReadAsString(buf->ReadableSize());
        LOGD("client recv:%s",s.c_str());
        conn->Shutdown();
    }

    void OnClosed(const PtrConnection& conn)
    {
        LOGD("close connection:%p",conn.get());
        exit(0);
    }

private:
    std::string _ip;
    uint16_t _port;
    LoopThread _loop_thread;
    PtrConnection _conn;
};
