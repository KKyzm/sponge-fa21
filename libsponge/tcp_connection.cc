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
    try_clean_shutdown();
}

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    if (!active())
        return;

    // reset timer
    _time_since_last_segment_received = 0;

    // check if the rst_flag work.
    if (seg.header().rst && (!in_listen() || seg.header().ack)) {
        unclean_shutdown(false);
        return;
    }

    // tells the _sender and _receiver what their care about
    if (seg.length_in_sequence_space() > 0)
        _receiver.segment_received(seg);
    if (seg.header().ack)
        _sender.ack_received(seg.header().ackno, seg.header().win);

    // reply syn if need
    if (seg.header().syn) {
        _sender.send_syn();
    }

    // reply the ackno and window_size to peer if need
    if (_receiver.ackno().has_value()) {
        if ((seg.length_in_sequence_space() > 0) ||
            (seg.header().seqno.raw_value() < _receiver.ackno().value().raw_value())) {
            if (_sender.segments_out().empty() && segments_out().empty())
                _sender.send_empty_segment();
        }
    }
    push_segments_out();
    try_clean_shutdown();
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
    if (!active())
        return;

    _time_since_last_segment_received += ms_since_last_tick;
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        unclean_shutdown();
    }
    push_segments_out();
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    push_segments_out();
}

void TCPConnection::connect() {
    if (!active())
        return;

    if (!_syn_sent) {
        _syn_sent = true;
        _sender.send_syn();
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

void TCPConnection::unclean_shutdown(bool send_rst) {
    // set in/out_stream error flag
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();

    if (send_rst) {
        // send_rst_segment
        TCPSegment rst_segment;
        rst_segment.header().seqno = _sender.next_seqno();
        rst_segment.header().rst = true;
        _segments_out.push(rst_segment);
    }

    // set TCPConnection active flag
    _active = false;
}
