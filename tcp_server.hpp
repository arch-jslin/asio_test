#ifndef ASIO_TEST_TCP_SERVER
#define ASIO_TEST_TCP_SERVER

#include <ctime>
#include <iostream>
#include <string>
#include <tr1/functional>
#include <tr1/memory>
#include <boost/enable_shared_from_this.hpp>
#include <boost/asio.hpp>

#include "helper.hpp"

using std::tr1::bind;
using boost::asio::ip::tcp;

class tcp_connection
  : public std::tr1::enable_shared_from_this<tcp_connection>
{
public:
  typedef std::tr1::shared_ptr<tcp_connection> pointer;

  static pointer create(boost::asio::io_service& io_service)
  {
    return pointer(new tcp_connection(io_service));
  }

  tcp::socket& socket()
  {
    return socket_;
  }

  void sendtime()
  {
    message_ = make_daytime_string();

    boost::asio::async_write(
        socket_, boost::asio::buffer(message_), bind(&tcp_connection::handle_write, shared_from_this()));
  }

private:
  tcp_connection(boost::asio::io_service& io_service)
    : socket_(io_service)
  {
  }

  void handle_write(/* neglected */)
  {
  }

  tcp::socket socket_;
  std::string message_;
};

class tcp_server
{
public:
  tcp_server(boost::asio::io_service& io_service)
    : acceptor_(io_service, tcp::endpoint(tcp::v4(), 13))
  {
    start_accept();
  }

private:
  void start_accept()
  {
    tcp_connection::pointer new_connection =
      tcp_connection::create(acceptor_.io_service());

    acceptor_.async_accept(new_connection->socket(),
      bind(&tcp_server::handle_accept, this, new_connection,
        std::tr1::placeholders::_1));
  }

  void handle_accept(tcp_connection::pointer new_connection,
      const boost::system::error_code& error)
  {
    if (!error)
    {
      new_connection->sendtime();
      start_accept();
    }
  }

  tcp::acceptor acceptor_;
};

#endif //ASIO_TEST_TCP_SERVER
