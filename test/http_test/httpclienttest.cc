#include "../../source/Server.hpp"
#include <signal.h>

using namespace server;

// 长连接测试：创建一个客户端持续给服务器发送数据，直到超过超时时间看看是否正常
void TimeoutTest1(Socket& client)
{
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
    while(1)
    {
        assert(client.Send(req.c_str(),req.size()) != -1);
        char buffer[1024] = {0};
        assert(client.Recv(buffer,1023));
        LOGD_STREAM() << buffer;

        sleep(3);
    }
}

// 超时连接测试：创建一个客户端，给服务器发送一次数据后，不动了，查看服务器是否会正常的超时关闭连接
void TimeoutTest2(Socket& client)
{
    while(1)
    {
        std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
        assert(client.Send(req.c_str(),req.size()) != -1);
        char buffer[1024] = {0};
        assert(client.Recv(buffer,1023));
        LOGD_STREAM() << buffer;

        sleep(15);
    }
}

// 给服务器发送一个数据，告诉服务器要发送1024字节的数据，但是实际发送的数据不足1024，查看服务器处理结果
/*
    1. 如果数据只发送一次，服务器将得不到完整请求，就不会进行业务处理，客户端也就得不到响应，最终超时关闭连接
    2. 连着给服务器发送了多次 小的请求，  服务器会将后边的请求当作前边请求的正文进行处理，而后面处理的时候有可能就会因为处理错误而关闭连接
*/

void SendCloseTest(Socket& client)
{
    std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 100\r\n\r\nbitejiuyeke";
    while(1)
    {
        assert(client.Send(req.c_str(),req.size()!=-1));
        assert(client.Send(req.c_str(),req.size()!=-1));
        assert(client.Send(req.c_str(),req.size()!=-1));
        char buf[1024] = {0};
        assert(client.Recv(buf,1023));
        LOGD_STREAM()<<"["<<buf<<"]";
        sleep(3);
    }
}

/* 业务处理超时，查看服务器的处理情况
    当服务器达到了一个性能瓶颈，在一次业务处理中花费了太长的时间（超过了服务器设置的非活跃超时时间）
     1. 在一次业务处理中耗费太长时间，导致其他的连接也被连累超时，其他的连接有可能会被拖累超时释放
     假设现在  12345描述符就绪了， 在处理1的时候花费了30s处理完，超时了，导致2345描述符因为长时间没有刷新活跃度
       1. 如果接下来的2345描述符都是通信连接描述符，如果都就绪了，则并不影响，因为接下来就会进行处理并刷新活跃度
       2. 如果接下来的2号描述符是定时器事件描述符，定时器触发超时，执行定时任务，就会将345描述符给释放掉
          这时候一旦345描述符对应的连接被释放，接下来在处理345事件的时候就会导致程序崩溃（内存访问错误）
          因此这时候，在本次事件处理中，并不能直接对连接进行释放，而应该将释放操作压入到任务池中，
          等到事件处理完了执行任务池中的任务的时候，再去释放
*/
// int main()
// {
//     signal(SIGCHLD, SIG_IGN);
//     for(int i=0;i<10;i++)
//     {
//         pid_t pid = fork();
//         if(pid<0)
//             return -1;
//         else if(pid==0)
//         {
//             Socket client;
//             client.CreateClient(8080,"127.0.0.1");
//             std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
//             while(1)
//             {
//                 assert(client.Send(req.c_str(),req.size()!=-1));
//                 char buf[1024] = {0};
//                 assert(client.Recv(buf,1023));
//                 LOGD_STREAM()<<buf;
//             }
//             client.Close();
//             exit(0);
//         }
//     }
//     while(1) sleep(1);

//     return 0;
// }


// 一次性给服务器发送多条数据，然后查看服务器的处理结果
// 每一条请求都应该得到正常处理
// void HandlerTest(Socket& client)
// {
//     std::string req = "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
//     req += "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
//     req += "GET /hello HTTP/1.1\r\nConnection: keep-alive\r\nContent-Length: 0\r\n\r\n";
//     while(1)
//     {
//         assert(client.Send(req.c_str(), req.size()) != -1);
//         char buf[1024] = {0};
//         assert(client.Recv(buf, 1023));
//         LOGD_STREAM() << buf;
//         sleep(3);
//     }
// }


int main()
{
    Socket client;
    client.CreateClient(8080,"127.0.0.1");

    // TimeoutTest1(client);
    // TimeoutTest2(client);

    // SendCloseTest(client);

    // HandlerTest(client);

    client.Close();

    return 0;
}