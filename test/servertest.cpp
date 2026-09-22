#include "../source/Server.hpp"
using namespace server_socket;

int main()
{
    Socket lis;
    lis.CreateServer(8090);
    while(1)
    {
        int newfd = lis.Accept();
        if(newfd<0) continue;
        Socket clie(newfd);
        char buf[1024] = {0};
        int ret = clie.Recv(buf,1023);
        if(ret<0)
        {
            clie.Close();
            continue;
        }
        clie.Send(buf,ret);
        clie.Close();
    }
    lis.Close();

    return 0;
}