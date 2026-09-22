#include "../source/Server.hpp"
using namespace server_socket;

int main()
{
    Socket cli;
    cli.CreateClient(8090,"127.0.0.1");
    std::string str = "hello world";
    cli.Send(str.c_str(),str.size());
    char buf[1024] = {0};
    cli.Recv(buf,1023);
    LOGD_STREAM() << buf;

    return 0;
}