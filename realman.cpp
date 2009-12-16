//actually this file was intended to be called "real_main"

//a playground for how to make-up a easy to use wrapper around
//Boost.ASIO, for the sake of simplicity in psc::net
//functionality currently is not main issue, ease-of-use is.

//this program has both the server and client functionality
//using command-line for testing only.

#include <iostream>
#include <string>
#include <tr1/functional>
#include <tr1/memory>
#include <boost/asio.hpp>
#include <boost/algorithm/string.hpp>
#include <boost/thread/thread.hpp>
#include <boost/thread/mutex.hpp>
#include <boost/lexical_cast.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#include "helper.hpp"

//must make a class for testing's sake.
class Peer {
    const short portin;
    const short portout;
    typedef std::tr1::shared_ptr< boost::thread > pThread;
public:

    static long GlobalMillisecond;
    static long GlobalMillisecond2;
    static LARGE_INTEGER last_counter_;

    Peer(const char* c_port)
      : portin(boost::lexical_cast<short>(c_port)),
        portout(portin+1), in_(io_), out_(io_), buffer_({0}), out_connected_(false),
        heartbeat_(io_, boost::posix_time::millisec(15)), heartbeat_counter_(0)
    {
        QueryPerformanceCounter(&last_counter_);
        showHelp();
        heartbeat_.async_wait( std::tr1::bind(&Peer::heartbeat_handler, this) );
    }

    ~Peer() {
        cleanUp();
        GlobalMillisecond += millisec_delta(last_counter_);
        std::cout << " -- Time Delta Accumulated(using performance counter): " << GlobalMillisecond << std::endl;
        std::cout << " -- Heartbeat count done by asio::deadline_timer: " << heartbeat_counter_ << std::endl;
        std::cout << " -- Average beat rate: " << (float)GlobalMillisecond / heartbeat_counter_ << std::endl;
    }

    void showHelp() const {
        std::cout << "\nBoost.ASIO Networking testing terminal: \n"
                  << " (/a) start listening on a port\n"
                  << " (/b) connecting to a host\n"
                  << " (/c) check status\n"
                  << " (/d) disconnect\n"
                  << " (/h) this option list\n"
                  << " (/q) leave\n"
                  << " anything other than above options will be transmitted as a message."
                  << std::endl;
    }

    void commandPrompt() const {
        std::cout << "> ";
    }

    bool run() {
        commandPrompt();
        char cmd[256];
        std::cin.getline(cmd, 256);
        bool stat = processCommand(cmd);

        return stat;
    }

    bool processCommand(char cmd[]) {
        if( cmd[0] == '/' ) {
            switch ( cmd[1] ) {
                case 'q':
                    cleanUp(); return false;
                case 'h':
                    showHelp(); return true;
                case 'd':
                    disconnect(); return true;
                case 'c':
                    showStatus(); return true;
                case 'b':
                    connectTo(); return true;
                case 'a':
                    listenTo(); return true;
                default:
                    return true;
            }
        }
        else {
            send(cmd);
            return true;
        }
    }

    void cleanUp() {
        disconnect();
    }

    void disconnect() {
        if( out_.is_open() || in_.is_open() ) {
            std::cout << "Sockets closed." << std::endl;
            out_.close();
            in_.close();
            io_.stop();
            asio_thread_->join();
            asio_thread_.reset();
            out_connected_ = false;
        }
    }

    void showStatus() {
    }

    void connectTo() {
        std::cout << "IP:Port> ";
        std::string in;
        std::cin >> in;
        std::vector<std::string> ip_port;

        using namespace boost::algorithm;
        split( ip_port, in, is_any_of(":") );
        std::cout << "Trying " << ip_port[0] << " " << ip_port[1] << "..." << std::endl;

        listenTo();
        connect_to(ip_port[0], boost::lexical_cast<short>(ip_port[1]));
    }

    void listenTo() {

        std::cout << "Start listening on port " << portin << "..." << std::endl;

        using namespace boost::asio::ip;
        in_.open(udp::v4());
        in_.bind(udp::endpoint(udp::v4(), portin));
        setup_handshake();
    }

    void send(char cmd[]) {
        using namespace std::tr1::placeholders;
        using std::tr1::bind;
        out_.async_send(boost::asio::buffer(cmd),
                        bind(&Peer::send_handler, this, std::tr1::placeholders::_1,
                             std::tr1::placeholders::_2));
    }

private:

    void heartbeat_handler() {
        ++heartbeat_counter_;

        GlobalMillisecond += millisec_delta(last_counter_);

        heartbeat_.expires_from_now( boost::posix_time::millisec(15) );
        heartbeat_.async_wait( std::tr1::bind(&Peer::heartbeat_handler, this) );
    }

    void setup_handshake() {

        io_.reset();

        using namespace std::tr1::placeholders;
        using std::tr1::bind;
        in_.async_receive_from(boost::asio::buffer(buffer_, 256),
                               remote_ep_,
                               bind(&Peer::handshake_handler, this,
                                    std::tr1::placeholders::_1,
                                    std::tr1::placeholders::_2));

        using std::tr1::bind;
        size_t (boost::asio::io_service::*fp)(void) = &boost::asio::io_service::run;
        if( !asio_thread_ )
            asio_thread_ = pThread( new boost::thread(bind(fp, &io_)) );
    }

    void setup_receive() {
        using namespace std::tr1::placeholders;
        using std::tr1::bind;
        for( int i = 1; i < 256; ++i ) buffer_[i] = 0;
        buffer_[0] = '\0';
        in_.async_receive(boost::asio::buffer(buffer_, 256),
                          bind(&Peer::receive_handler, this, std::tr1::placeholders::_1,
                               std::tr1::placeholders::_2));
    }

    void connect_to(std::string const& ip, short const& port) {
        using namespace boost::asio::ip;

        udp::resolver resolver(io_);
        udp::resolver::query query(udp::v4(), ip, boost::lexical_cast<std::string>(port));
        udp::endpoint dest = *resolver.resolve(query);

        using std::tr1::bind;
        out_.open(udp::v4());
        out_.bind(udp::endpoint(udp::v4(), portout));
        out_.async_connect(dest, bind(&Peer::connect_handler, this, std::tr1::placeholders::_1));
    }

    void connect_handler(boost::system::error_code const& ec) {
        if( !ec ) {
            std::cout << "Trace: Connected." << std::endl;
            out_connected_ = true;
            send("Greetings.");
        }
        else {
            std::cerr << "Connect handling error: " << ec.message() << std::endl;
        }
    }

    void receive_handler(boost::system::error_code const& ec,
                        std::size_t const& bytes_transferred)
    {
        //GlobalMillisecond += millisec_delta(last_counter_);

        if( !ec ) {
            std::cout << "Message coming in: \"" << buffer_ << "\"";
        }
        else {
            std::cout << "Receive handling error: " << ec.message();
        }
        std::cout << ", bytes_transferred: " << bytes_transferred << std::endl;

        setup_receive();
    }

    void handshake_handler(boost::system::error_code const& ec,
                           std::size_t const& bytes_transferred)
    {
        //GlobalMillisecond += millisec_delta(last_counter_);

        if( !ec ) {
            std::cout << "A peer connected from " << remote_ep_.address().to_string() << ":"
                      << remote_ep_.port();
            std::cout << " " << buffer_;
        }
        else {
            std::cout << "Handshake handling error: " << ec.message();
        }
        std::cout << ", bytes_transferred: " << bytes_transferred << std::endl;

        if( !out_connected_ )
            connect_to(remote_ep_.address().to_string(), remote_ep_.port()-1);

        setup_receive();
    }

    void send_handler(boost::system::error_code const& ec,
                     std::size_t const& bytes_transferred) {
        if( !ec ) {
            std::cout << "Message sent";
        }
        else {
            std::cout << "Send handling error: " << ec.message();
        }
        std::cout << ", bytes_transferred: " << bytes_transferred << std::endl;
    }

    boost::asio::io_service io_;
    boost::asio::ip::udp::socket in_;
    boost::asio::ip::udp::socket out_;

    boost::asio::ip::udp::endpoint remote_ep_;
    boost::asio::deadline_timer heartbeat_;
    pThread asio_thread_;

    int heartbeat_counter_;

    char buffer_[256];
    bool out_connected_;
};

long Peer::GlobalMillisecond = 0;
long Peer::GlobalMillisecond2 = 0;
LARGE_INTEGER Peer::last_counter_;

int main(int argc, char* argv[])
{
    Peer p( argc == 2 ? argv[1] : "12345" );
    while( p.run() );

    return 0;
}
