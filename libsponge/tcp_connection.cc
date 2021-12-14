#include "tcp_connection.hh"

#include <algorithm>
#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPConnection::push_segments_out() {
    if (!active())
        return;

    TCPSegment seg;
    while (!_sender.segments_out().empty()) {
        seg = _sender.segments_out().front();
        _sender.segments_out().pop();

        if (_receiver.ackno().has_value()) {
            seg.header().ack = true;
            seg.header().ackno = _receiver.ackno().value();
            seg.header().win = min(_receiver.window_size(), static_cast<size_t>(numeric_limits<uint16_t>::max()));
        }
        _segments_out.push(seg);
    }
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active())
        return;

    _time_since_last_segment_received = 0;

    if (seg.header().rst) {
        unclean_shutdown();
        return;
    }

    if (!_syn_received) {
        if (seg.header().syn == true)
            _syn_received = true;
        else
            return;
    }

    if (seg.length_in_sequence_space() > 0)
        _receiver.segment_received(seg);
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);

    _inbound_end = _receiver.stream_out().input_ended();
    _outbound_end = _sender.stream_in().input_ended();
    _outbound_acked = _outbound_end & _sender.fully_acked();

    if (_receiver.ackno().has_value()) {
        if (seg.length_in_sequence_space() > 0) {
            if (_sender.segments_out().empty())
                _sender.send_empty_segment();
            push_segments_out();
        } else if (seg.header().seqno.raw_value() < _receiver.ackno().value().raw_value()) {
            if (_sender.segments_out().empty())
                _sender.send_empty_segment();
            push_segments_out();
        }
    }

    if (!_sender.get_fin() && seg.header().fin)
        _linger_after_streams_finish = false;

    if (_inbound_end && _outbound_end && _outbound_acked) {
        if (!_linger_after_streams_finish ||
            (_linger_after_streams_finish && time_since_last_segment_received() >= 10 * _cfg.rt_timeout))
            _active = false;
    }
}

bool TCPConnection::active() const { return _active; }

size_t TCPConnection::write(const string &data) {
    if (!active())
        return 0;
    size_t size = _sender.stream_in().write(data);
    _sender.fill_window();
    push_segments_out();
    return size;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    if (!_active)
        return;

    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown();
    }
}

void TCPConnection::end_input_stream() { _sender.stream_in().end_input(); }

void TCPConnection::connect() {
    if (!active())
        return;

    if (!_syn_sent) {
        _syn_sent = true;
        _sender.fill_window();
        push_segments_out();
    }
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";

            // Your code here: need to send a RST segment to the peer
            unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::unclean_shutdown() {
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    // send_rst_segment
    TCPSegment rst_segment;
    rst_segment.header().seqno = _sender.next_seqno();
    rst_segment.header().rst = true;
    _segments_out.push(rst_segment);

    _active = false;
}
