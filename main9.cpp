
#include "udp_server_custom.hpp"
#include "tcp_server.hpp"
#include <windows.h>

using std::tr1::bind;
using std::tr1::ref;

class client
{
public:
  client(boost::asio::io_service& io, char const* addr)
    : strand_(io),
      timer1_(io, boost::posix_time::seconds(1)),
      timer2_(io, boost::posix_time::seconds(1)),
      count_(0), addr_(addr)
  {
    timer1_.async_wait(strand_.wrap(bind(&client::print1, this)));
    timer2_.async_wait(strand_.wrap(bind(&client::print2, this)));
  }

  ~client()
  {
    std::cout << "Final count is " << count_ << "\n";
  }

  void print1()
  {
    if (count_ < 30)
    {
      std::cout << "Timer 1: " << count_ << "\n";
      ++count_;

      timer1_.expires_at(timer1_.expires_at() + boost::posix_time::seconds(1));
      timer1_.async_wait(strand_.wrap(bind(&client::print1, this)));
    }
    else strand_.get_io_service().stop();
  }

  void print2()
  {
    if (count_ < 30)
    {
      if( addr_[0] != '\0' )
        ping();

      timer2_.expires_at(timer2_.expires_at() + boost::posix_time::seconds(1));
      timer2_.async_wait(strand_.wrap(bind(&client::print2, this)));
    }
    else strand_.get_io_service().stop();
  }

private:
  void ping() {
    udp::resolver resolver(strand_.get_io_service());
    udp::resolver::query query(udp::v4(), addr_, "daytime");
    udp::endpoint receiver_endpoint = *resolver.resolve(query);
    receiver_endpoint.port(12345);

    udp::socket socket(strand_.get_io_service());
    socket.open(udp::v4());

    std::cout << "socket open... sending ping request." << std::endl;

    boost::array<LARGE_INTEGER, 1> send_buf;
    QueryPerformanceCounter(&send_buf[0]);
    //send_buf[0] = time(0);
    socket.send_to(boost::asio::buffer(send_buf), receiver_endpoint);

    boost::array<char, 128> recv_buf;
    udp::endpoint sender_endpoint;
    size_t len = socket.receive_from(
      boost::asio::buffer(recv_buf), sender_endpoint);

    LARGE_INTEGER now, freq;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&freq);

    std::cout.write(recv_buf.data(), len);
    std::cout << " -- " << (now.QuadPart - send_buf[0].QuadPart) * 1000000 / freq.QuadPart << ", at freq " << freq.QuadPart << std::endl;
  }
  boost::asio::strand strand_;
  boost::asio::deadline_timer timer1_;
  boost::asio::deadline_timer timer2_;
  int count_;
  char const* addr_;
};


int main(int argc, char* argv[])
{
    try {
        boost::asio::io_service asio;
        tcp_server* serv1 = 0;
        udp_server_custom* serv2 = 0;
        if( argc <= 1 ) {
            std::cout << "receiver server constructed.\n";
            serv1 = new tcp_server(asio);
            serv2 = new udp_server_custom(asio);
        }
        client c(asio, (argc > 1) ? argv[1] : "");
        asio.run();
        std::cout << "hihihi?." << std::endl;
        if( serv1 != 0 ) delete serv1;
        if( serv2 != 0 ) delete serv2;
    }
    catch(std::exception& e) {
        std::cerr << e.what() << std::endl;
    }
    return 0;
}
