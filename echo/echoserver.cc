#include "EchoServer.hpp"

void Usage(std::string procname)
{
    std::cout << "Usage: " << procname << " ServerPort" << std::endl;
}

int main(int argc,char* argv[])
{
    if(argc!=2)
    {
        Usage(argv[0]);
        exit(1);
    }

    uint16_t port = std::stoi(argv[1]);
    std::shared_ptr<EchoServer> server = std::make_shared<EchoServer>(port);
    server->Start();

    return 0;
}