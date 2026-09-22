#pragma  once
#include <iostream>
#include <string>
#include <cstring>
#include <netinet/in.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <unistd.h>
#include <cerrno>

#include "../log/Log.hpp"

namespace server_socket
{
    using namespace LogModule;
    
    #define MAX_LISTEN 1024
    class Socket
    {
    public:
        Socket(int fd = -1):_socketfd(fd) {}
        int Fd() { return _socketfd; }

        // 创建套接字
        bool Create()
        {
            // int socket(int domain, int type, int protocol)
            _socketfd = socket(AF_INET,SOCK_STREAM,IPPROTO_TCP);
            if(_socketfd < 0)
            {
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Create socket failed!";
                return false; 
            }
            return true;
        }

        // 绑定地址信息
        bool Bind(const std::string& ip, uint16_t port)
        {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());
            socklen_t len = sizeof(struct sockaddr_in);
            // int bind(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
            int n = bind(_socketfd,(struct sockaddr*)&addr,len);
            if(n<0)
            {
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Bind address failed!";
                return false;
            }
            return true;
        }

        // 连接服务器
        bool Connect(const std::string& ip, uint16_t port)
        {
            struct sockaddr_in addr;
            memset(&addr, 0, sizeof(addr));
            addr.sin_family = AF_INET;
            addr.sin_port = htons(port);
            addr.sin_addr.s_addr = inet_addr(ip.c_str());
            socklen_t len = sizeof(struct sockaddr_in);
            // int connect(int sockfd, const struct sockaddr *addr, socklen_t addrlen);
            int n = connect(_socketfd,(struct sockaddr*)&addr,len);
            if(n<0)
            {
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Connect server failed!";
                return false;
            }
            return true;
        }

        // 开始监听
        bool Listen(int backlog = MAX_LISTEN)
        {
            // int listen(int sockfd, int backlog);
            int n = listen(_socketfd,backlog);
            if(n<0)
            {
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Socket listen failed!";
                return false;
            }
            return true;
        }

        // 获取新连接
        int Accept()
        {
            // int accept(int sockfd, struct sockaddr *addr, socklen_t *len);
            int newfd = accept(_socketfd,nullptr,nullptr);
            if(newfd < 0)
            {
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Socket accept failed!";
                return -1;
            }
            return newfd;
        }

        // 接收数据
        ssize_t Recv(void* buf, size_t len, int flag = 0)
        {
            // ssize_t recv(int sockfd, void *buf, size_t len, int flag);
            ssize_t n = recv(_socketfd,buf,len,flag);
            if(n <= 0)
            {
                // EAGAIN 当前socket的接收缓冲区中没有数据了，在非阻塞的情况下才会有这个错误
                // EINTR  表示当前socket的阻塞等待，被信号打断了，
                if(errno == EAGAIN || errno == EINTR)
                    return 0;
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Socket recv failed!";
                return -1;
            }
            return n;
        }

        ssize_t NonBlockRecv(void* buf, size_t len)
        {
            return Recv(buf,len,MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
        }

        // 发送数据
        ssize_t Send(const void* buf,size_t len, int flag = 0)
        {
            // ssize_t send(int sockfd, void *data, size_t len, int flag);
            ssize_t ret = send(_socketfd,buf,len,flag);
            if(ret < 0)
            {
                if(errno == EAGAIN || errno == EINTR)
                    return 0;
                LOG_ERROR_STREAM(GetLogger("ServerLogger")) << "Socket send failed!";
                return -1;
            }
            return ret; // 实际发送的数据长度
        }

        ssize_t NonBlockSend(const void* buf, size_t len)
        {
            if(!len) return 0;
            return Send(buf,len,MSG_DONTWAIT); // MSG_DONTWAIT 表示当前接收为非阻塞。
        }

        // 关闭套接字
        void Close()
        {
            if(_socketfd != -1)
            {
                close(_socketfd);
                _socketfd = -1;
            }
        }

        // 设置套接字阻塞属性-- 设置为非阻塞
        void NonBlock()
        {
            // int fcntl(int fd, int cmd, ... /* arg */ );
            int flag = fcntl(_socketfd,F_GETFL,0);
            fcntl(_socketfd,F_SETFL,flag | O_NONBLOCK);
        }

        // 设置套接字选项---开启地址端口重用
        void ReuseAddress()
        {
            // int setsockopt(int fd, int leve, int optname, void *val, int vallen)
            int val = 1;
            setsockopt(_socketfd,SOL_SOCKET,SO_REUSEADDR,(void*)&val,sizeof(int));
            val = 1;
            setsockopt(_socketfd,SOL_SOCKET,SO_REUSEADDR,(void*)&val,sizeof(int));
        }

        // 创建一个服务端连接
        bool CreateServer(uint16_t port, const std::string& ip = "0.0.0.0", bool block_flag = false)
        {
            // 1. 创建套接字，2. 绑定地址，3. 开始监听，4. 设置非阻塞， 5. 启动地址重用
            if(!Create())       return false;
            if(block_flag)      NonBlock();
            if(!Bind(ip,port))  return false;
            if(!Listen())       return false;
            ReuseAddress();
            return true;
        }

        // 创建一个客户端连接
        bool CreateClient(uint16_t port, const std::string& ip)
        {
            // 1. 创建套接字，2.指向连接服务器
            if(!Create())           return false;
            if(!Connect(ip,port))   return false;
            return true;
        }

        ~Socket() { Close(); }

    private:
        int _socketfd;
    };
}