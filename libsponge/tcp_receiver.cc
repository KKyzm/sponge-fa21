#include "tcp_receiver.hh"

#include "wrapping_integers.hh"

#include <memory>

// Dummy implementation of a TCP receiver

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    auto seg_header = seg.header();
    auto seg_body = seg.payload();

    // Init ISN of this TCP connect receiver.
    if (seg_header.syn) {
        if (_syn)
            return;
        _syn = true;
        _isn = seg_header.seqno.raw_value();
    } else if (!_syn)
        return;

    if (seg_header.fin) {
        if (_fin)
            return;
        _fin = true;
    }

    // Checkpoint is the index of the last reassembled byte
    // while ack is the index of the first unassembled byte.
    uint64_t seg_checkpoint = static_cast<uint64_t>(_reassembler.get_first_unassembled() - 1) + 1;
    uint64_t index =
        unwrap(seg_header.seqno + static_cast<uint32_t>(seg_header.syn), WrappingInt32(_isn), seg_checkpoint);

    // Only the part of the string that is subscripted after checkpoint is transmitted.
    if (index > 0) {
        index--;
        _reassembler.push_substring(seg_body.copy(), index, seg_header.fin);
    } else {
        if (((seg_checkpoint - index) + 1) < seg_body.copy().size())
            _reassembler.push_substring(
                seg_body.copy().substr((seg_checkpoint - index) + 1), seg_checkpoint, seg_header.fin);
        else
            _reassembler.push_substring("", seg_checkpoint, seg_header.fin);
    }
    return;
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_syn)
        return WrappingInt32(static_cast<uint32_t>(_reassembler.get_first_unassembled() + static_cast<size_t>(_syn) +
                                                   static_cast<size_t>(_reassembler.stream_out().input_ended())) +
                             _isn);
    else
        return std::nullopt;
}

size_t TCPReceiver::window_size() const { return _capacity - _reassembler.stream_out().buffer_size(); }
