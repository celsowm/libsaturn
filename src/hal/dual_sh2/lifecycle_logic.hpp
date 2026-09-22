#ifndef SATURN_HAL_DUAL_SH2_LIFECYCLE_LOGIC_HPP
#define SATURN_HAL_DUAL_SH2_LIFECYCLE_LOGIC_HPP

#include <stdint.h>

namespace saturn::hal::dual_sh2::lifecycle_logic {

enum class State : uint8_t { Offline, Starting, Ready, Stopping, Fault };
enum class Event : uint8_t { StartIssued, StartAcknowledged, StopRequested,
                             StopAcknowledged, Timeout, Reset };

inline State transition(State state, Event event) {
    switch (event) {
        case Event::StartIssued:
            return state == State::Offline ? State::Starting : State::Fault;
        case Event::StartAcknowledged:
            return state == State::Starting ? State::Ready : State::Fault;
        case Event::StopRequested:
            return (state == State::Ready || state == State::Starting)
                ? State::Stopping : State::Fault;
        case Event::StopAcknowledged:
            return state == State::Stopping ? State::Offline : State::Fault;
        case Event::Timeout:
            return (state == State::Starting || state == State::Stopping)
                ? State::Fault : state;
        case Event::Reset:
            return state == State::Fault ? State::Offline : state;
    }
    return State::Fault;
}

inline bool timeout_elapsed(uint16_t start, uint16_t now, uint16_t timeout) {
    return static_cast<uint16_t>(now - start) >= timeout;
}

}  // namespace saturn::hal::dual_sh2::lifecycle_logic

#endif
