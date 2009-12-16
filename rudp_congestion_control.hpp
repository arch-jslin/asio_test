#ifndef _SHOOTING_CUBES_NET_DETAIL_RUDP_CONGESTION_CONTROL_
#define _SHOOTING_CUBES_NET_DETAIL_RUDP_CONGESTION_CONTROL_

namespace net {
namespace detail {

class CongestionControl
{
public:
    CongestionControl();
    void reset();
    void update( float const& delta_t, float const& rtt );
    double getSendRate();

private:
    enum State { Good, Bad };
    State state_;

    double penalty_time_;
    double good_conditions_time_;
    double penalty_reduction_accumulator_;
};

} //detail
} //net

#endif //_SHOOTING_CUBES_NET_DETAIL_RUDP_CONGESTION_CONTROL_

