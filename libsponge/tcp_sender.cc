#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "wrapping_integers.hh"

#include <random>

// Dummy implementation of a TCP sender

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    // , _initial_retransmission_timeout{retx_timeout}
    , _initial_retransmission_timeout(retx_timeout)
    , _retransmission_limiter(retx_timeout)
    , _retransmission_timer(0)
    , _timer_toggle(false)
    , _consecutive_retransmissions(0)
    , _window_size(0)
    , _bytes_in_flight(0)
    , _abs_ackno(0)
    , _received_ackno(0)
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const { return _bytes_in_flight; }

void TCPSender::fill_window() {
    if (send_syn())
        return;

    size_t real_window_size = _window_size;
    if (real_window_size == 0 && _invoke) {
        _invoke = false;
        TCPSegment invoke_seg;
        if (!stream_in().buffer_empty())
            invoke_seg.payload() = stream_in().read(1);
        else if (stream_in().eof())
            invoke_seg.header().fin = true;
        else
            return;

        invoke_seg.set_invoke();
        send_segment(invoke_seg);
    } else if (real_window_size == 0 && !_invoke) {
        return;
    } else {
        if (_received_ackno + real_window_size < _next_seqno)
            return;

        if (_next_seqno > _received_ackno)
            real_window_size -= (_next_seqno - _received_ackno);

        while (!_fin && real_window_size > 0) {
            size_t bytes_to_read = min(TCPConfig::MAX_PAYLOAD_SIZE, real_window_size);
            string str_tmp = stream_in().read(bytes_to_read);

            TCPSegment mesg_seg_tmp;
            mesg_seg_tmp.payload() = Buffer(std::move(str_tmp));

            if (!_fin && mesg_seg_tmp.length_in_sequence_space() < real_window_size && stream_in().eof()) {
                mesg_seg_tmp.header().fin = true;
                _fin = true;
            }

            if (mesg_seg_tmp.length_in_sequence_space() == 0)
                return;
            send_segment(mesg_seg_tmp);
            real_window_size -= mesg_seg_tmp.length_in_sequence_space();
        }
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    _received_ackno = unwrap(ackno, _isn, _abs_ackno);
    if (_received_ackno > _next_seqno)
        return;
    //! When the receiver gives the sender an ackno that acknowledges the successful receipt of new data
    //  (the ackno reflects an absolute sequence number bigger than any previous ackno)
    if (_abs_ackno < _received_ackno) {
        _abs_ackno = _received_ackno;
        RTO_reset();
        _consecutive_retransmissions = 0;

        //! The TCPSender should look through its collection of outstanding segments
        //  and remove any that have now been fully acknowledged
        //  (the ackno is greater than all of the sequence numbers in the seg)
        TCPSegment seg_tmp;
        while (!_segments_outstanding.empty()) {
            seg_tmp = _segments_outstanding.front();
            uint64_t seg_absno = unwrap(seg_tmp.header().seqno, _isn, _next_seqno);
            uint64_t seg_length = static_cast<uint64_t>(seg_tmp.length_in_sequence_space());
            if (seg_absno + seg_length <= _abs_ackno) {
                _bytes_in_flight -= seg_tmp.length_in_sequence_space();
                _segments_outstanding.pop();
                timer_turn_up();
            } else
                break;
        }
    }

    _window_size = window_size;

    if (window_size == 0)
        _invoke = true;
    fill_window();

    if (_segments_outstanding.empty())
        timer_turn_off();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (!_timer_toggle)
        return;

    _retransmission_timer += ms_since_last_tick;
    if (time_out() && !_segments_outstanding.empty()) {
        TCPSegment seg_retrans = _segments_outstanding.front();
        _segments_out.push(seg_retrans);
        _consecutive_retransmissions++;
        if (!seg_retrans.get_invoke())
            RTO_mul2();
        timer_turn_up();
    } else if (_segments_outstanding.empty()) {
        timer_turn_off();
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions; }

void TCPSender::send_empty_segment() {
    TCPSegment empty_segment;
    empty_segment.header().seqno = next_seqno();
    _segments_out.push(empty_segment);
}

void TCPSender::send_segment(TCPSegment &seg) {
    seg.header().seqno = next_seqno();
    _next_seqno += seg.length_in_sequence_space();
    _bytes_in_flight += seg.length_in_sequence_space();
    _segments_out.push(seg);
    _segments_outstanding.push(seg);
    if (!_timer_toggle)
        timer_turn_up();
}
