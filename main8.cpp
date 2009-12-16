
#include "tcp_server.hpp"
#include "udp_server.hpp"

int main()
{
    try {
        boost::asio::io_service asio;
        tcp_server serv1(asio);
        udp_server serv2(asio);
        asio.run();
    }
    catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
