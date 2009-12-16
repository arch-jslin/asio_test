
#include <iostream>
#include <boost/date_time/posix_time/posix_time.hpp>

int main()
{
    using boost::posix_time::ptime;
    using boost::posix_time::microsec_clock;

    for( int i = 0; i < 100000; ++i ) {
        ptime t(microsec_clock::universal_time());
        std::cout << to_iso_string(t) << std::endl;
    }

    return 0;
}
