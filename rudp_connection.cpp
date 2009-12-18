
#include "helper.hpp"
#include "rudp_packet.hpp"
#include "rudp_connection.hpp"
#include <sstream>
#include <algorithm>

//The thread object and the IO project probably fits better in upper scope,
//Since it's not a per-connection things.

//There are somethings potentially not thread-safe in this implementation,
//however the occuring possibility is quite low. For example,
//maybe you assigned a new onRecv callback concurrently with recv_cb_ being
//executed at the same time. (must beware of those read/write on buffers as well)
//But for now I try not to add complexity by introducing mutex and lock here.

using namespace net;
using namespace detail;
namespace tr1_ph = std::tr1::placeholders;

long Connection::GlobalMillisecond = 0;   //these will be extracted to upper scope
#ifdef WIN32
LARGE_INTEGER Connection::last_counter_;  //these will be extracted to upper scope
#endif

/// *********** public methods ***********

Connection::Connection(unsigned short const& protocol_id)
    :socket_(io_), keep_io_running_( new boost::asio::io_service::work(io_) ),
     heartbeat_(io_), timeout_(io_), keepalive_(io_), recvbuffer_({0}), sendbuffer_({0}),
     out_connected_(false), protocol_id_(protocol_id), HANDSHAKE_CHAR({'\1',0}), KEEPALIVE_CHAR({'\2',0}),
     state_(Disconnected)
{
    std::cerr << "Trace: Connection object created with id " << protocol_id_ << "\n";
#ifdef WIN32
    QueryPerformanceCounter(&last_counter_);
#endif
    timer(heartbeat_, boost::posix_time::millisec(15),
        std::tr1::bind(&Connection::heartbeat_handler, this, tr1_ph::_1) );

    size_t (boost::asio::io_service::*fp)(void) = &boost::asio::io_service::run;
    asio_thread_ = pThread( new boost::thread( std::tr1::bind(fp, &io_) ) );
}

Connection::~Connection() {
    disconnect();
    clean_up();
    keep_io_running_.reset(); //so the io_service::work dies, make io::run stoppable.
    io_.stop();
    asio_thread_->join();
    std::cerr << "Trace: Connection Object died gracefully. Performance Counter: "
              << GlobalMillisecond << "\n";
}

bool Connection::start(unsigned short const& port) {
    std::cerr << "Trace: Start listening on port " << port << "...\n";

    using namespace boost::asio::ip;

    if( !socket_.is_open() )
        socket_.open(udp::v4());
    try {
        socket_.bind(udp::endpoint(udp::v4(), port));
    }
    catch( std::exception& e ) {
        std::cerr << e.what() << "\n";
        return false;
    }
    setup_handshake();
    state_ = Listening;
    return true;
}

bool Connection::connect(char const* ip, unsigned short const& port) {
    using namespace boost::asio::ip;

    std::ostringstream oss;
    oss << port;
    udp::resolver resolver(io_);
    udp::resolver::query query(udp::v4(), ip, oss.str());
    udp::endpoint dest;
    try {
        dest = *resolver.resolve(query);
    }
    catch( std::exception& e ){
        std::cerr << e.what() << "\n";
        return false;
    }

    using std::tr1::bind;
    socket_.async_connect(dest, bind(&Connection::connect_handler, this, tr1_ph::_1));

    timer(timeout_, boost::posix_time::seconds(3),
        bind(&Connection::timeout_handler, this, tr1_ph::_1) );

    state_ = Connecting;
    return true;
}

void Connection::disconnect() {
    if( socket_.is_open() ) {
        std::cerr << "Trace: Sockets closed.\n";

        io_.post(std::tr1::bind(&Connection::do_socket_close, this)); //so this will be called in asio_thread_

        out_connected_ = false;
        state_ = Disconnected;
        if( disconnect_cb_ )
            disconnect_cb_();
    }
}

void Connection::send(char packet[]) {
    attach_header(sendbuffer_);
    int i = /*sizeof(PacketHeader)*/ + sizeof(unsigned short);
    int j = 0; //if sizeof(packet) > 256 - i then it will overflow
    while( packet[j] != '\0' && i < 255 ) {
        sendbuffer_[i] = packet[j];
        ++i, ++j;
    }
    io_.post(std::tr1::bind(&Connection::do_send, this)); //so this will be called in asio_thread_
}

void Connection::onDisconnect(std::tr1::function<void()> cb) { disconnect_cb_ = cb; }
void Connection::onConnect(std::tr1::function<void()> cb)    { connect_cb_ = cb; }
void Connection::onRecv(std::tr1::function<void(char*)> cb)  { recv_cb_ = cb; }

/// **** private methods and handlers ****

void Connection::attach_header(char packet[]) {
    packet[0] = static_cast<char>( protocol_id_ >> 8 );
    packet[1] = static_cast<char>( protocol_id_ & 0x00FF );
}

PacketHeader Connection::detach_header(char raw_packet[], char packet[]) {
    int i = /*sizeof(PacketHeader)*/ + sizeof(unsigned short);
    int j = 0;
    while( raw_packet[i] != '\0' && i < 255 ) {
        packet[j] = raw_packet[i];
        ++i, ++j;
    }
    return PacketHeader();
}

bool Connection::protocol_filter(char const packet[]) {
    unsigned short protocol_id = (packet[0] << 8) | packet[1];
    if( protocol_id != protocol_id_ )
        return false;
    //we might add other checkings here.
    return true;
}

void Connection::clean_up() {
    flush(recvbuffer_, 256);
    flush(sendbuffer_, 256);
}

void Connection::flush(char* buffer_to_be_flushed, size_t const& size) {
    for( int i = 0; i < size; ++i ) buffer_to_be_flushed[i] = 0;
}

void Connection::setup_handshake() {
    using std::tr1::bind;
    flush(recvbuffer_, 256);
    socket_.async_receive_from(boost::asio::buffer(recvbuffer_, 256), remote_ep_,
                               bind(&Connection::handshake_handler, this, tr1_ph::_1, tr1_ph::_2));
}

void Connection::setup_receive() {
    using std::tr1::bind;
    flush(recvbuffer_, 256);
    socket_.async_receive(boost::asio::buffer(recvbuffer_, 256),
                          bind(&Connection::receive_handler, this, tr1_ph::_1, tr1_ph::_2));
}

void Connection::timeout_handler(boost::system::error_code const& ec) {
    if( ec == boost::asio::error::operation_aborted ) //we are sure that this is caused by timer::cancel
        return;

    std::cerr << "Trace: Connection timed out or remote stopped running.\n";
    disconnect(); //possible thread-safety problem if we call this here.
}

void Connection::keepalive_handler(boost::system::error_code const& ec) {
    if( ec == boost::asio::error::operation_aborted ) //we are sure that this is caused by timer::cancel
        return;

    using std::tr1::bind; using boost::posix_time::seconds;
    send(KEEPALIVE_CHAR); //this is a placeholder for keepalive packet
    timer( keepalive_, seconds(1), bind(&Connection::keepalive_handler, this, tr1_ph::_1) );
}

void Connection::heartbeat_handler(boost::system::error_code const& ec) {
    if( ec == boost::asio::error::operation_aborted ) //we are sure that this is caused by timer::cancel
        return;
#ifdef WIN32
    GlobalMillisecond += millisec_delta(last_counter_); //always remember: it is not accurate
#endif
    timer( heartbeat_, boost::posix_time::millisec(15),
        std::tr1::bind(&Connection::heartbeat_handler, this, tr1_ph::_1) );
}

void Connection::connect_handler(boost::system::error_code const& ec) {
    if( !ec ) {
        std::cerr << "Trace: Socket Connected.\n";
        out_connected_ = true;
        send(HANDSHAKE_CHAR); //this is a handshake packet placeholder
        timer(keepalive_, boost::posix_time::seconds(1),
            std::tr1::bind(&Connection::keepalive_handler, this, tr1_ph::_1) );
        state_ = Connected;
        if( connect_cb_ )
            connect_cb_();
    }
    else {
        std::cerr << "Trace: " << ec.message() << std::endl;
        state_ = Listening;
    }
}

void Connection::receive_handler(boost::system::error_code const& ec, std::size_t const& size) {
    //GlobalMillisecond += millisec_delta(last_counter_);
    if( !ec ) {
        if( !protocol_filter(recvbuffer_) ) {
            std::cerr << "Trace: An unknown terminal was attempting to send messages while "
                      << "the current connection was still alive. Denied.\n";
            setup_receive();
            return;
        }
        char packetbody[256] = {0}; //this will be the packet without header
        std::cerr << "Trace: Message coming in -- \"" << recvbuffer_ << "\"";
        do_reset_timeout();
        PacketHeader header = detach_header(recvbuffer_, packetbody);

        if( *packetbody != '\2' && recv_cb_ )
            recv_cb_(packetbody);

        setup_receive();
    }
    else if( ec == boost::asio::error::operation_aborted )
        std::cerr << "Trace: Socket operations aborted forcefully because socket is closed";
    else {
        std::cerr << "Trace: Receive handling error: " << ec.message();
        disconnect(); //possible thread-safety problem if we call this here.
    }

    std::cerr << ", bytes_transferred: " << size << "\n";
}

void Connection::handshake_handler(boost::system::error_code const& ec, std::size_t const& size) {
    //GlobalMillisecond += millisec_delta(last_counter_);
    if( !ec ) {
        if( !protocol_filter(recvbuffer_) ) {
            std::cerr << "Trace: An unknown terminal from " << remote_ep_.address().to_string() << ":"
                      << remote_ep_.port() << " was attempting to make connection. Denied.\n";
            setup_handshake();
            return;
        }
        std::cerr << "Trace: A peer connected from " << remote_ep_.address().to_string() << ":"
                  << remote_ep_.port() << ", buffer: " << recvbuffer_;
        do_reset_timeout();
        //no recv_cb_ here?
        if( !out_connected_ )
            connect(remote_ep_.address().to_string().c_str(), remote_ep_.port());

        std::cerr << ", bytes_transferred: " << size << "\n";
        setup_receive();
    }
    else if( ec == boost::asio::error::operation_aborted )
        std::cerr << "Trace: Socket operations aborted forcefully because socket is closed";
    else {
        std::cerr << "Trace: Handshake handling error: " << ec.message();
        disconnect(); //possible thread-safety problem if we call this here.
    }

    std::cerr << ", bytes_transferred: " << size << "\n";
}

void Connection::send_handler(boost::system::error_code const& ec, std::size_t const& size) {
    if( !ec ) std::cerr << "Trace: Message sent: " << sendbuffer_;
    else      std::cerr << "Trace: Send handling error: " << ec.message();
    std::cerr << ", bytes_transferred: " << size << "\n";

    flush(sendbuffer_, 256);
}

void Connection::do_socket_close() {
    timeout_.cancel();
    keepalive_.cancel();
    socket_.close();
}

void Connection::do_send() {
    using std::tr1::bind;
    socket_.async_send(boost::asio::buffer(&sendbuffer_[0]),
                       bind(&Connection::send_handler, this, tr1_ph::_1, tr1_ph::_2));
   /* Why the hell &sendbuffer_[0] ?
      because if you use "sendbuffer_" alone, compiler well resolved to this overloading:

      template< typename PodType, std::size_t N >
      mutable_buffers_1 buffer( PodType (&data)[N], std::size_t max_size_in_bytes);

      which means that size is going to be fixed on size of the buffer array you send in!
      and no matter what's in the buffer, socket_.send() will always send that fixed size!!
   */
}

void Connection::do_reset_timeout() {
    timeout_.cancel(); //important, this action will invoke the callback nonetheless.
    timer(timeout_, boost::posix_time::seconds(3),
        std::tr1::bind(&Connection::timeout_handler, this, tr1_ph::_1) );
}

template <typename Callback>
void Connection::timer(boost::asio::deadline_timer& timer,
    boost::posix_time::time_duration const& t, Callback cb)
{
    timer.expires_from_now( t );
    timer.async_wait(cb); //asio.timers don't recognize tr1 function -__-, have to do it this way.
}
