#include "tcp_sender.hh"

#include "tcp_config.hh"
#include "wrapping_integers.hh"

#include <algorithm>
#include <random>

// For Lab 3, please replace with a real implementation that passes the
// automated checks run by `make check_lab3`.

using namespace std;

//! \param[in] capacity the capacity of the outgoing byte stream
//! \param[in] retx_timeout the initial amount of time to wait before retransmitting the oldest outstanding segment
//! \param[in] fixed_isn the Initial Sequence Number to use, if set (otherwise uses a random ISN)
TCPSender::TCPSender(const size_t capacity, const uint16_t retx_timeout, const std::optional<WrappingInt32> fixed_isn)
    : _isn(fixed_isn.value_or(WrappingInt32{random_device()()}))
    , _init_timer_backup{retx_timeout}
    , _initial_retransmission_timeout{retx_timeout}
    , _stream(capacity) {}

uint64_t TCPSender::bytes_in_flight() const {
    uint64_t total_bytes_in_flight = 0;
    for (auto it = _segments_outstanding.begin(); it != _segments_outstanding.end(); ++it) {
        total_bytes_in_flight += it->second.length_in_sequence_space();
    }
    return total_bytes_in_flight;
}

void TCPSender::fill_window() {
    // send segments until _last_ackno + _window_size == _next_seqno,
    // AKA until the window of TCPreceiver is fully filled

    if (_fin_sent == true) {
        // TCPSender state == FIN_SENT or FIN_ACKED
        return;
    }
    if (next_seqno_absolute() == 0) {
        // TCPSender state == CLOSED
        TCPSegment seg;
        seg.header().syn = true;
        send_segment(seg, false);
    }

    uint64_t num_bytes_to_send;
    if (_last_ackno + _window_size >= _next_seqno) {
        num_bytes_to_send = _last_ackno + _window_size - _next_seqno;
    } else {
        num_bytes_to_send = 0;
    }

    bool window_zero_flag = false;
    if (_window_size == 0 && _window_zero_valid == true) {
        num_bytes_to_send++;
        window_zero_flag = true;
        _window_zero_valid = false;
    }

    while (num_bytes_to_send > 0) {
        TCPSegment seg;

        size_t max_read_length = min(num_bytes_to_send, TCPConfig::MAX_PAYLOAD_SIZE);
        string payload = stream_in().read(max_read_length);
        size_t real_read_length = payload.length();
        num_bytes_to_send -= real_read_length;

        seg.payload() = Buffer(std::move(payload));
        if (stream_in().eof() && num_bytes_to_send > 0 && _fin_sent == false) {
            seg.header().fin = true;
            _fin_sent = true;
            num_bytes_to_send--;
        }

        if (seg.length_in_sequence_space() == 0)
            break;
        send_segment(seg, window_zero_flag);
    }
}

//! \param ackno The remote receiver's ackno (acknowledgment number)
//! \param window_size The remote receiver's advertised window size
void TCPSender::ack_received(const WrappingInt32 ackno, const uint16_t window_size) {
    if (_next_seqno == 0)
        return;

    // update the _last_ackno and _window_size
    uint64_t new_ackno = unwrap(ackno, _isn, _last_ackno);
    uint64_t new_window_size = window_size;

    // ignore impossible ackno
    if (new_ackno > _next_seqno)
        return;

    _last_ackno = max(_last_ackno, new_ackno);
    if (new_ackno + new_window_size > _last_ackno) {
        _window_size = new_ackno + new_window_size - _last_ackno;
    } else {
        _window_size = 0;
        _window_zero_valid = true;
    }

    remove_outstanding_segments();
    fill_window();
}

//! \param[in] ms_since_last_tick the number of milliseconds since the last call to this method
void TCPSender::tick(const size_t ms_since_last_tick) {
    if (_timer_enable == false)
        return;

    _retransmission_timeout += ms_since_last_tick;
    if (_retransmission_timeout >= _initial_retransmission_timeout) {
        timeout_retransmission();
        // reset timer
        _timer_enable = true;
        _retransmission_timeout = 0;
    }
}

unsigned int TCPSender::consecutive_retransmissions() const { return _consecutive_retransmissions_count; }

void TCPSender::send_empty_segment() {
    TCPSegment seg;
    send_segment(seg, false);
}

void TCPSender::send_empty_segment_with_this_header(TCPHeader header) {
    TCPSegment seg;
    seg.header() = header;
    send_segment(seg, false);
}

void TCPSender::send_segment(TCPSegment &seg, bool window_zero_flag) {
    // auto fill the seqno field of seg
    uint64_t abs_seqno = next_seqno_absolute();
    WrappingInt32 wrap_seqno = next_seqno();
    seg.header().seqno = wrap_seqno;
    _next_seqno += seg.length_in_sequence_space();
    _segments_out.push(seg);

    // do not retransmit empty segment
    if (seg.length_in_sequence_space() != 0) {
        _timer_enable = true;
        _segments_outstanding.push_back(make_pair(make_pair(abs_seqno, window_zero_flag), seg));
    }
}

void TCPSender::remove_outstanding_segments() {
    bool segments_poped_out = false;

    while (!_segments_outstanding.empty()) {
        auto seq_seg_pair = _segments_outstanding.front();
        uint64_t abs_seqno = seq_seg_pair.first.first;
        TCPSegment seg = seq_seg_pair.second;
        if (abs_seqno + seg.length_in_sequence_space() <= _last_ackno) {
            _segments_outstanding.pop_front();
            segments_poped_out = true;
        } else {
            break;
        }
    }

    if (segments_poped_out == true) {
        // reset timer
        _retransmission_timeout = 0;
        _initial_retransmission_timeout = _init_timer_backup;

        if (_segments_outstanding.empty() == true)
            _timer_enable = false;
        else
            _timer_enable = true;

        // reset consecutive retransmissions counter
        _consecutive_retransmissions_count = 0;
    }
}

void TCPSender::timeout_retransmission() {
    if (_segments_outstanding.empty() == true) {
        _timer_enable = false;
        _retransmission_timeout = 0;
        _initial_retransmission_timeout = _init_timer_backup;
        _consecutive_retransmissions_count = 0;
        return;
    }
    // retransmit the first outstanding segment
    bool window_zero_flag = _segments_outstanding.front().first.second;
    TCPSegment seg_timeout = _segments_outstanding.front().second;
    // _segments_outstanding.push_back(_segments_outstanding.front());
    // _segments_outstanding.pop_front();
    _segments_out.push(seg_timeout);

    if (window_zero_flag == false) {
        _initial_retransmission_timeout *= 2;
        _consecutive_retransmissions_count++;
    }
}
