#include "EchoClient.hpp"

void Usage(std::string procname)
{
    std::cout << "Usage: " << procname << " ServerIp ServerPort" << std::endl;
}

int main(int argc,char* argv[])
{
    if(argc!=3)
    {
        Usage(argv[0]);
        exit(1);
    }

    std::string ip = argv[1];
    uint16_t port = std::stoi(argv[2]);
    std::shared_ptr<EchoClient> client = std::make_shared<EchoClient>(ip,port);
    client->Start();

    return 0;
}
