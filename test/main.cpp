
#define _WIN32_WINNT 0x0501  //XP

#include <iostream>
#include <tr1/functional>
#include <boost/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

using std::tr1::bind;
using std::tr1::ref;

void sync_timer_test()
{
    boost::asio::io_service io;
    boost::asio::deadline_timer t(io, boost::posix_time::seconds(5));

    t.wait();

    std::cout << "Hello, world!\n";
}

void print(boost::system::error_code const&) {
    std::cout << "Hello, world!\n";
}

void print2(boost::system::error_code const&,
    boost::asio::deadline_timer* t, int& count)
{
    using namespace std::tr1::placeholders;
    if (count < 5)
    {
        std::cout << count << "\n";
        ++count;

        t->expires_at(t->expires_at() + boost::posix_time::seconds(1));
        t->async_wait( bind(&print2, _1, t, ref(count)) );
    }
}


void async_timer_test()
{
    boost::asio::io_service io;
    boost::asio::deadline_timer t(io, boost::posix_time::seconds(5));

    using boost::system::error_code;
    t.async_wait( &print );
    io.run();
}

void async_timer_test2()
{
    using namespace std::tr1::placeholders;

    boost::asio::io_service io;
    int count = 0;
    boost::asio::deadline_timer t(io, boost::posix_time::seconds(1));
    t.async_wait( bind(&print2, _1, &t, ref(count)) );

    io.run();

    std::cout << "Final count is " << count << "\n";
}

class printer
{
public:
    printer(boost::asio::io_service& io)
        : timer_(io, boost::posix_time::seconds(1)),
          count_(0)
    {
        timer_.async_wait(bind(&printer::print, this));
    }

    ~printer()
    {
        std::cout << "Final count is " << count_ << "\n";
    }

    void print()
    {
        if (count_ < 5)
        {
            std::cout << count_ << "\n";
            ++count_;

            timer_.expires_at(timer_.expires_at() + boost::posix_time::seconds(1));
            timer_.async_wait(bind(&printer::print, this));
        }
    }

private:
    boost::asio::deadline_timer timer_;
    int count_;
};


int main()
{
    async_timer_test2();

    boost::asio::io_service io;
    printer p(io);
    io.run();

    return 0;
}
