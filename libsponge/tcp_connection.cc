#include "tcp_connection.hh"

#include <iostream>
#include <limits>

// Dummy implementation of a TCP connection

// For Lab 4, please replace with a real implementation that passes the
// automated checks run by `make check`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

size_t TCPConnection::remaining_outbound_capacity() const { return _sender.stream_in().remaining_capacity(); }

size_t TCPConnection::bytes_in_flight() const { return _sender.bytes_in_flight(); }

size_t TCPConnection::unassembled_bytes() const { return _receiver.unassembled_bytes(); }

size_t TCPConnection::time_since_last_segment_received() const { return _time_since_last_segment_received; }

void TCPConnection::segment_received(const TCPSegment &seg) {
    // return if the connection is done
    if (active() == false)
        return;

    // reset timer
    _time_since_last_segment_received = 0;

    // unclean shutdown if RST flag is set in received segment
    if (seg.header().rst == true) {
        _sender.stream_in().set_error();
        _receiver.stream_out().set_error();
        return;
    }

    // no linger time if FIN flag comes before _sender end
    if (seg.header().fin == true && _sender.stream_in().input_ended() == false) {
        _linger_after_streams_finish = false;
    }

    // give the necessary information to both _receiver and _sender
    _receiver.segment_received(seg);
    if (seg.header().ack == true) {
        _sender.ack_received(seg.header().ackno, seg.header().win);
    }

    // respond to keep-alive segment
    if (_receiver.ackno().has_value() && (seg.length_in_sequence_space() == 0) &&
        seg.header().seqno == _receiver.ackno().value() - 1) {
        _sender.send_empty_segment();
    }

    // respond to any non-empty segments
    if (seg.length_in_sequence_space() != 0) {
        _sender.fill_window();
        if (_sender.segments_out().empty()) {
            _sender.send_empty_segment();
        }
    }

    pop_and_send_segments();
}

bool TCPConnection::active() const {
    ByteStream outbound = _sender.stream_in();
    ByteStream inbound = _receiver.stream_out();
    if (outbound.eof() && inbound.eof()) {
        if (_sender.bytes_in_flight() == 0) {
            if (_linger_after_streams_finish == false)
                return false;
            else
                return time_since_last_segment_received() < 10 * _cfg.rt_timeout;
        } else
            return true;
    }
    if (outbound.error() || inbound.error())
        return false;

    return true;
}

size_t TCPConnection::write(const string &data) {
    // write data into outbound stream
    size_t total_write_length = _sender.stream_in().write(data);
    _sender.fill_window();
    pop_and_send_segments();

    return total_write_length;
}

//! \param[in] ms_since_last_tick number of milliseconds since the last call to this method
void TCPConnection::tick(const size_t ms_since_last_tick) {
    _sender.tick(ms_since_last_tick);
    if (_sender.consecutive_retransmissions() > TCPConfig::MAX_RETX_ATTEMPTS) {
        positive_unclean_shutdown();
    }
    pop_and_send_segments();

    _time_since_last_segment_received += ms_since_last_tick;
}

void TCPConnection::end_input_stream() {
    _sender.stream_in().end_input();
    _sender.fill_window();
    pop_and_send_segments();
}

void TCPConnection::connect() {
    _sender.fill_window();
    pop_and_send_segments();
}

TCPConnection::~TCPConnection() {
    try {
        if (active()) {
            cerr << "Warning: Unclean shutdown of TCPConnection\n";
            positive_unclean_shutdown();
        }
    } catch (const exception &e) {
        std::cerr << "Exception destructing TCP FSM: " << e.what() << std::endl;
    }
}

void TCPConnection::pop_and_send_segments() {
    numeric_limits<uint16_t> uint16_limit;
    size_t win_size = min(_receiver.window_size(), static_cast<size_t>(uint16_limit.max()));

    while (!_sender.segments_out().empty()) {
        TCPSegment seg = _sender.segments_out().front();
        _sender.segments_out().pop();
        if (_receiver.ackno().has_value() && seg.header().ack == false) {
            seg.header().ackno = _receiver.ackno().value();
            seg.header().ack = true;
        }
        if (seg.header().win == TCPHeader().win)
            seg.header().win = static_cast<uint16_t>(win_size);
        _segments_out.push(seg);
    }
}

void TCPConnection::positive_unclean_shutdown() {
    // send empty segment with RST flag set
    TCPHeader header;
    header.rst = true;
    while (!_sender.segments_out().empty()) {
        _sender.segments_out().pop();
    }
    _sender.send_empty_segment_with_this_header(header);
    pop_and_send_segments();
    // set both ByteStream to error state
    _sender.stream_in().set_error();
    _receiver.stream_out().set_error();
}
