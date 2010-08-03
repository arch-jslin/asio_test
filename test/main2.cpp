
#include <iostream>
#include <tr1/functional>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using std::tr1::bind;
using std::tr1::ref;

class printer
{
public:
  printer(boost::asio::io_service& io)
    : strand_(io),
      timer1_(io, boost::posix_time::seconds(1)),
      timer2_(io, boost::posix_time::seconds(1)),
      count_(0)
  {
    timer1_.async_wait(strand_.wrap(bind(&printer::print1, this)));
    timer2_.async_wait(strand_.wrap(bind(&printer::print2, this)));
  }

  ~printer()
  {
    std::cout << "Final count is " << count_ << "\n";
  }

  void print1()
  {
    if (count_ < 10)
    {
      std::cout << "Timer 1: " << count_ << "\n";
      ++count_;

      timer1_.expires_at(timer1_.expires_at() + boost::posix_time::seconds(1));
      timer1_.async_wait(strand_.wrap(bind(&printer::print1, this)));
    }
  }

  void print2()
  {
    if (count_ < 20)
    {
      std::cout << "Timer 2: " << count_ << "\n";
      ++count_;

      timer2_.expires_at(timer2_.expires_at() + boost::posix_time::seconds(1));
      timer2_.async_wait(strand_.wrap(bind(&printer::print2, this)));
    }
  }

private:
  boost::asio::strand strand_;
  boost::asio::deadline_timer timer1_;
  boost::asio::deadline_timer timer2_;
  int count_;
};

int main()
{
  boost::asio::io_service io;
  printer p(io);
  size_t (boost::asio::io_service::*fp)(void) = &boost::asio::io_service::run; //for overloaded func
  boost::thread t(bind(fp, &io));

  for( int i = 0 ; i < 1000000000; ++i ) { //main thread blocking test
      if( i % 100000000 == 0 )
          std::cout << "tick... " << std::endl;
  }

  io.run();
  t.join();
  return 0;
}

