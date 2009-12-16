
#include <iostream>
#include "rudp_congestion_control.hpp"

using namespace net;
using namespace detail;

CongestionControl::CongestionControl()
{
    std::cerr << "Trace: congestion control initialized.\n";
    reset();
}

void CongestionControl::reset()
{
    state_ = Bad;
    penalty_time_ = 4.0;
    good_conditions_time_ = 0.0;
    penalty_reduction_accumulator_ = 0.0;
}

void CongestionControl::update( float const& delta_t, float const& rtt )
{
    const float rtt_threshold = 250.0f;

    if ( state_ == Good ) {
        if ( rtt > rtt_threshold ) {
            std::cerr << "Trace: state switched to bad mode.\n";
            state_ = Bad;
            if ( good_conditions_time_ < 10.0 && penalty_time_ < 60.0 ) {
                penalty_time_ *= 2.0;
                if ( penalty_time_ > 60.0 )
                    penalty_time_ = 60.0;
                std::cerr << "Trace: penalty time increased to " << penalty_time_ << "\n";
            }
            good_conditions_time_ = 0.0;
            penalty_reduction_accumulator_ = 0.0;
            return;
        }

        good_conditions_time_ += delta_t;
        penalty_reduction_accumulator_ += delta_t;

        if ( penalty_reduction_accumulator_ > 10.0 && penalty_time_ > 1.0 ) {
            penalty_time_ /= 2.0;
            if ( penalty_time_ < 1.0 )
                penalty_time_ = 1.0;
            std::cerr << "Trace: penalty time reduced to " << penalty_time_ << "\n";
            penalty_reduction_accumulator_ = 0.0;
        }
    }

    if ( state_ == Bad )
    {
        if ( rtt <= rtt_threshold )
            good_conditions_time_ += delta_t;
        else
            good_conditions_time_ = 0.0;

        if ( good_conditions_time_ > penalty_time_ ) {
            std::cerr << "Trace: state switched to good mode.\n";
            good_conditions_time_ = 0.0;
            penalty_reduction_accumulator_ = 0.0;
            state_ = Good;
            return;
        }
    }
}

double CongestionControl::getSendRate()
{
    return state_ == Good ? 30.0 : 10.0;
}
