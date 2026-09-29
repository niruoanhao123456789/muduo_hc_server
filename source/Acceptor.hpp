#pragma once
#include "Socket.hpp"
#include "Channel.hpp"
#include "EventLoop.hpp"

namespace server_acceptor
{
    using namespace server_socket;
    using namespace server_channel;
    using namespace server_eventloop;

    class Acceptor
    {
    using AcceptCallBack = std::function<void(int)>;
    public:
        Acceptor(EventLoop* loop,int port)
        :_socket(CreateServer(port))
        ,_loop(loop)
        ,_channel(_loop,_socket.Fd())
        {
            _channel.SetReadCallBack(std::bind(&Acceptor::HandleRead,this));
            _channel.EnableRead();
        }

        void SetAcceptCallBack(const AcceptCallBack& cb)
        {
            _accept_cb = cb;
        }
        
    private:
        void HandleRead()
        {
            int fd = _socket.Accept();
            if(fd<0)
                return;
            if(_accept_cb)
                _accept_cb(fd);
        }

        int CreateServer(int port)
        {
            bool ret= _socket.CreateServer(port);
            assert(ret);
            
            return _socket.Fd();
        }

    private:
        Socket _socket;
        EventLoop* _loop;
        Channel _channel;
        AcceptCallBack _accept_cb;
    };
}