#ifndef ASIO_TEST_UDP_SERVER
#define ASIO_TEST_UDP_SERVER

#include <ctime>
#include <iostream>
#include <string>
#include <tr1/functional>
#include <tr1/memory>
#include <boost/array.hpp>
#include <boost/asio.hpp>

#include "helper.hpp"

using boost::asio::ip::udp;

class udp_server
{
public:
  udp_server(boost::asio::io_service& io_service)
    : socket_(io_service, udp::endpoint(udp::v4(), 13))
  {
    start_receive();
  }

private:
  void start_receive()
  {
    socket_.async_receive_from(
      boost::asio::buffer(recv_buffer_), remote_endpoint_,
      std::tr1::bind(&udp_server::handle_receive, this, std::tr1::placeholders::_1,
        std::tr1::placeholders::_2) );
  }

  void handle_receive(const boost::system::error_code& error,
      std::size_t /*bytes_transferred*/)
  {
    if (!error || error == boost::asio::error::message_size)
    {
      std::tr1::shared_ptr<std::string> message(
          new std::string(make_daytime_string()));

      socket_.async_send_to(boost::asio::buffer(*message), remote_endpoint_,
        std::tr1::bind(&udp_server::handle_send, this, message) );

      start_receive();
    }
  }

  void handle_send(std::tr1::shared_ptr<std::string> message)
  {
    std::cout << *message << std::endl;
  }

  udp::socket socket_;
  udp::endpoint remote_endpoint_;
  boost::array<char, 1> recv_buffer_;
};

#endif //ASIO_TEST_UDP_SERVER


