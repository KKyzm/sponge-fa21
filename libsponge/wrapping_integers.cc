#include "wrapping_integers.hh"

#include <algorithm>
#include <iostream>

// Dummy implementation of a 32-bit wrapping integer

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! Transform an "absolute" 64-bit sequence number (zero-indexed) into a WrappingInt32
//! \param n The input absolute 64-bit sequence number
//! \param isn The initial sequence number
WrappingInt32 wrap(uint64_t n, WrappingInt32 isn) { return isn + static_cast<uint32_t>(n); }

//! Transform a WrappingInt32 into an "absolute" 64-bit sequence number (zero-indexed)
//! \param n The relative sequence number
//! \param isn The initial sequence number
//! \param checkpoint A recent absolute 64-bit sequence number
//! \returns the 64-bit sequence number that wraps to `n` and is closest to `checkpoint`
//!
//! \note Each of the two streams of the TCP connection has its own ISN. One stream
//! runs from the local TCPSender to the remote TCPReceiver and has one ISN,
//! and the other stream runs from the remote TCPSender to the local TCPReceiver and
//! has a different ISN.
uint64_t unwrap(WrappingInt32 n, WrappingInt32 isn, uint64_t checkpoint) {
    uint64_t absolute_seq = static_cast<uint64_t>(n.raw_value() - isn.raw_value());
    if (checkpoint <= absolute_seq)
        return absolute_seq;

    absolute_seq += checkpoint & 0xFFFFFFFF00000000;
    if (checkpoint > absolute_seq && checkpoint - absolute_seq > UINT32_MAX / 2) {
        absolute_seq += UINT32_MAX;
        absolute_seq++;
    } else if (absolute_seq > checkpoint && absolute_seq - checkpoint > UINT32_MAX / 2) {
        absolute_seq -= UINT32_MAX;
        absolute_seq--;
    }
    return absolute_seq;

    // WrappingInt32 absolute_low = WrappingInt32(n.raw_value() - isn.raw_value());
    // if (checkpoint <= absolute_low.raw_value())
    //     return static_cast<uint64_t>(absolute_low.raw_value());
    //
    // WrappingInt32 checkpoint_low = WrappingInt32(static_cast<uint32_t>(checkpoint));
    // uint32_t tmp = absolute_low.raw_value() - checkpoint_low.raw_value();
    // uint32_t delta = min(tmp, UINT32_MAX + 1 - tmp);
    //
    // if (absolute_low == checkpoint_low)
    //     return checkpoint;
    // else if ((absolute_low.raw_value() > checkpoint_low.raw_value() &&
    //           absolute_low.raw_value() - checkpoint_low.raw_value() <= UINT32_MAX / 2) ||
    //          (absolute_low.raw_value() < checkpoint_low.raw_value() &&
    //           checkpoint_low.raw_value() - absolute_low.raw_value() > UINT32_MAX / 2)) {
    //     return checkpoint + delta;
    // } else {
    //     return checkpoint - delta;
    // }
}
