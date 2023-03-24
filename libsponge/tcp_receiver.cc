#include "tcp_receiver.hh"

// For Lab 2, please replace with a real implementation that passes the
// automated checks run by `make check_lab2`.

using namespace std;

void TCPReceiver::segment_received(const TCPSegment &seg) {
    bool syn = seg.header().syn;
    bool fin = seg.header().fin;
    WrappingInt32 seqno = seg.header().seqno;
    string payload = seg.payload().copy();

    // offset between absolute seqno and stream no (equals stream no - absolute seqno)
    int offset_stream_no = -1;

    // handle with seg's syn flag only when _syn is not set
    if (_syn_recived == false) {
        if (syn == true) {
            _syn_recived = true;
            _isn = seqno.raw_value();
            offset_stream_no = 0;
        } else {
            return;
        }
    }

    // convert seqno to absolute seqno
    uint64_t abs_seqno = unwrap(seg.header().seqno, WrappingInt32(_isn), _last_stream_no);
    uint64_t stream_no = abs_seqno + offset_stream_no;

    _last_stream_no = stream_no;
    _reassembler.push_substring(payload, stream_no, fin);
}

optional<WrappingInt32> TCPReceiver::ackno() const {
    if (_syn_recived == false)
        return {};
    // _syn == true, which means _isn must have been set correctly

    uint64_t stream_no = static_cast<uint64_t>(_reassembler.get_next_index());
    uint64_t offset_ackno = (_syn_recived ? 1 : 0) + (_reassembler.stream_out().input_ended() ? 1 : 0);
    // stream_no is the ackno's next stream index, it should be converted to seqno
    return wrap(stream_no + offset_ackno, WrappingInt32(_isn));
}

size_t TCPReceiver::window_size() const { return stream_out().remaining_capacity(); }
