#ifndef ASIO_TEST_UDP_SERVER_2
#define ASIO_TEST_UDP_SERVER_2

#include <ctime>
#include <iostream>
#include <string>
#include <tr1/functional>
#include <tr1/memory>
#include <boost/array.hpp>
#include <boost/asio.hpp>
#include <sstream>

#include "helper.hpp"
#include <windows.h>

//this function is useless
char const* make_ping_value_str(LARGE_INTEGER t) {
    std::ostringstream oss;
    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);
    oss << ((now.QuadPart - t.QuadPart) * 1000000 / freq.QuadPart);
    std::cout << oss.str() << std::endl;
    return oss.str().c_str();
}

using boost::asio::ip::udp;

class udp_server_custom
{
public:
  udp_server_custom(boost::asio::io_service& io_service)
    : socket_(io_service, udp::endpoint(udp::v4(), 12345))
  {
    start_receive();
  }

private:
  void start_receive()
  {
    socket_.async_receive_from(
      boost::asio::buffer(recv_buffer_), remote_endpoint_,
      std::tr1::bind(&udp_server_custom::handle_receive, this, std::tr1::placeholders::_1,
        std::tr1::placeholders::_2) );
  }

  void handle_receive(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/)
  {
    if (!error || error == boost::asio::error::message_size)
    {
      std::cout << "I received a ping request. (check msg in: " << recv_buffer_[0].QuadPart << ")" << std::endl;

      std::tr1::shared_ptr<std::string> message(
          new std::string(make_ping_value_str(recv_buffer_[0])));

      socket_.async_send_to(boost::asio::buffer(*message), remote_endpoint_,
        std::tr1::bind(&udp_server_custom::handle_send, this, message) );

      start_receive();
    }
  }

  void handle_send(std::tr1::shared_ptr<std::string> message)
  {
    std::cout << "I am sending: " << *message << std::endl;
  }

  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  boost::array<LARGE_INTEGER, 1> recv_buffer_;
};

#endif //ASIO_TEST_UDP_SERVER_2



