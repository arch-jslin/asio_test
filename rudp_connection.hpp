#ifndef _SHOOTINC_CUBES_NET_DETAIL_RUDP_CONNECTION_
#define _SHOOTINC_CUBES_NET_DETAIL_RUDP_CONNECTION_

#include <tr1/functional>
#include <tr1/memory>
#include <boost/asio.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>

#ifdef WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace net {
namespace detail {

struct PacketHeader;

class Connection
{
    typedef std::tr1::shared_ptr< boost::thread > pThread;
    typedef std::tr1::shared_ptr< boost::asio::io_service::work > pASIOWork;

    static long GlobalMillisecond; //these will be extracted to upper scope
#ifdef WIN32
    static LARGE_INTEGER last_counter_; //these will be extracted to upper scope
#endif

public:
    Connection(unsigned short const& protocol_id);
    ~Connection();

    bool start(unsigned short const& port);
    bool connect(char const* ip, unsigned short const& port);
    void disconnect();

    void send(char packet[]);
//    void sendReliable(char packet[]);

    void onConnect(std::tr1::function<void()> cb);
    void onDisconnect(std::tr1::function<void()> cb);
    void onRecv(std::tr1::function<void(char*)> cb);
//    void onRecvReliable(std::tr1::function<void(char const*)> cb);

    bool isDisconnected() const { return state_ == Disconnected; }
    bool isListening()    const { return state_ == Listening; }
    bool isConnecting()   const { return state_ == Connecting; }
    bool isConnected()    const { return state_ == Connected; }

protected: //asio handlers
    void setup_handshake();
    void setup_receive();
    void flush(char* buffer_to_be_flushed, size_t const& size);
    void clean_up();
    void attach_header(char packet[]);
    PacketHeader detach_header(char raw_packet[], char packet[]);
    bool protocol_filter(char const packet[]);

    void timeout_handler(boost::system::error_code const& ec);
    void keepalive_handler(boost::system::error_code const& ec);
    void heartbeat_handler(boost::system::error_code const& ec);
    void resolve_handler(boost::system::error_code const& ec,
                         boost::asio::ip::udp::resolver::iterator dest_iterator);
    void connect_handler(boost::system::error_code const& ec);
    void receive_handler(boost::system::error_code const& ec, std::size_t const& size);
    void handshake_handler(boost::system::error_code const& ec, std::size_t const& size);
    void send_handler(boost::system::error_code const& ec, std::size_t const& size);

    void do_connect();
    void do_socket_close();
    void do_send();
    void do_reset_timeout();

    template<typename Callback>
    void timer(boost::asio::deadline_timer& timer,
               boost::posix_time::time_duration const& t,
               Callback cb);

protected: //members
    boost::asio::io_service io_;
    pASIOWork keep_io_running_;
    boost::asio::ip::udp::socket socket_;
    boost::asio::ip::udp::endpoint remote_ep_;

    boost::asio::deadline_timer heartbeat_;
    boost::asio::deadline_timer timeout_;
    boost::asio::deadline_timer keepalive_;
    //Possibly need to be subsituted if we need higher precision.

    pThread asio_thread_; //these will be extracted to upper scope

    char recvbuffer_[256];
    char sendbuffer_[256];
    bool out_connected_;
    unsigned short protocol_id_;
    char HANDSHAKE_CHAR[2], KEEPALIVE_CHAR[2];
    std::string dest_ip_;
    unsigned short dest_port_;

    enum State{ Disconnected, Listening, Connecting, Connected };
    State state_;

protected: //real-world handlers
    std::tr1::function<void()> connect_cb_, disconnect_cb_;
    std::tr1::function<void(char*)> recv_cb_;
};

} //net
} //detail

#endif //_SHOOTINC_CUBES_NET_DETAIL_RUDP_CONNECTION_
