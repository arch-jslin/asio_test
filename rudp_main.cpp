
#include <iostream>
#include <boost/algorithm/string.hpp>

#include "rudp_congestion_control.hpp"
#include "rudp_connection.hpp"
#include "rudp_reliability_system.hpp"

using namespace net;
namespace tr1_ph = std::tr1::placeholders;

class Peer {
    short port;

public:
    Peer(char const* c_port)
      : port(boost::lexical_cast<short>(c_port)), conn_((('C'+'u')-('B'*'e')/'a'%'t')|0x0101)
    {                                                  //above is magical number for protocol id
        showHelp();
        while( !conn_.start(port) ) {
            ++port; //it's a cheap way to avoid port conflict, but it works for simple cases.
            std::cout << "Now new port is: " << port << std::endl;
        }

        using std::tr1::bind;
        conn_.onRecv(bind(&Peer::receive, this, tr1_ph::_1));
    }

    ~Peer() {
    }

    void showHelp() const {
        std::cout << "\nBoost.ASIO Networking testing terminal: \n"
                  << " (/a) start running on port " << port << "\n"
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
        char cmd[256] = {0};
        std::cin.getline(cmd, 256);
        bool stat = processCommand(cmd);

        return stat;
    }

    bool processCommand(char cmd[]) {
        if( cmd[0] == '/' ) {
            switch ( cmd[1] ) {
                case 'q':
                    return false;
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

    void listenTo() {
        if( conn_.isDisconnected() ) {
            while( !conn_.start(port) )
                ++port;
        }
    }

    void showStatus() {}

    void disconnect() {
        if( conn_.isConnected() )
            conn_.disconnect();
    }

    void connectTo() {
        if( conn_.isConnected() || conn_.isConnecting() ) return;

        std::cout << "IP:Port> ";
        std::string in;
        std::cin >> in;
        std::vector<std::string> ip_port;

        using namespace boost::algorithm;
        split( ip_port, in, is_any_of(":") );

        listenTo();
        conn_.connect(ip_port[0].c_str(), boost::lexical_cast<short>(ip_port[1]));
    }

    void send(char cmd[]) {
        if( conn_.isConnected() )
            conn_.send(cmd);
    }

    void receive(char cmd[]) {
        std::cout << "Somebody said: " << cmd << std::endl;
    }

private:

    detail::Connection conn_;
};

int main(int argc, char* argv[])
{
//The timer will have to do for now. I can change it to using EventDispatcher's Timer when
//in the production code.

//The new issue is that I must write my own reliability system and congestion control
//base on Gaffer's experiences. Should not be too hard to implement,
//but I still need to break the parts down, especially that I have to write my own sliding window first.
    Peer p( argc == 2 ? argv[1] : "12345" );
    while( p.run() );

    return 0;
}
