#ifndef SPONGE_LIBSPONGE_TCP_SENDER_HH
#define SPONGE_LIBSPONGE_TCP_SENDER_HH

#include "byte_stream.hh"
#include "tcp_config.hh"
#include "tcp_segment.hh"
#include "wrapping_integers.hh"

#include <functional>
#include <queue>

//! \brief The "sender" part of a TCP implementation.

//! Accepts a ByteStream, divides it up into segments and sends the
//! segments, keeps track of which segments are still in-flight,
//! maintains the Retransmission Timer, and retransmits in-flight
//! segments if the retransmission timer expires.
class TCPSender {
  private:
    //! our initial sequence number, the number for our SYN.
    WrappingInt32 _isn;

    //! outbound queue of segments that the TCPSender wants sent
    std::queue<TCPSegment> _segments_out{};
    std::queue<TCPSegment> _segments_outstanding{};

    //! retransmission timer for the connection
    const unsigned int _initial_retransmission_timeout;
    unsigned int _retransmission_limiter;
    unsigned int _retransmission_timer;
    bool _timer_toggle;

    unsigned int _consecutive_retransmissions;
    size_t _window_size;
    unsigned int _bytes_in_flight;
    uint64_t _abs_ackno;
    uint64_t _received_ackno;
    bool _syn = false;
    bool _fin = false;
    bool _invoke = false;

    //! outgoing stream of bytes that have not yet been sent
    ByteStream _stream;

    //! the (absolute) sequence number for the next byte to be sent
    uint64_t _next_seqno{0};

  public:
    bool send_syn() {
        if (!_syn) {
            TCPSegment synseg;
            synseg.header().syn = true;
            send_segment(synseg);
            _syn = true;
            return true;
        } else
            return false;
    }
    bool fully_acked() const { return _abs_ackno == _next_seqno; }
    bool get_fin() { return _fin; }

    unsigned int get_timer() const { return _retransmission_timer; }
    void timer_turn_up() {
        _timer_toggle = true;
        _retransmission_timer = 0;
    }
    void timer_turn_off() { _timer_toggle = false; }
    void RTO_mul2() { _retransmission_limiter *= 2; }
    void RTO_reset() { _retransmission_limiter = _initial_retransmission_timeout; }
    bool time_out() {
        if (_retransmission_timer >= _retransmission_limiter && _timer_toggle)
            return true;
        else
            return false;
    }

    //! Initialize a TCPSender
    TCPSender(const size_t capacity = TCPConfig::DEFAULT_CAPACITY,
              const uint16_t retx_timeout = TCPConfig::TIMEOUT_DFLT,
              const std::optional<WrappingInt32> fixed_isn = {});

    void send_segment(TCPSegment &seg);

    //! \name "Input" interface for the writer
    //!@{
    ByteStream &stream_in() { return _stream; }
    const ByteStream &stream_in() const { return _stream; }
    //!@}

    //! \name Methods that can cause the TCPSender to send a segment
    //!@{

    //! \brief A new acknowledgment was received
    void ack_received(const WrappingInt32 ackno, const uint16_t window_size);

    //! \brief Generate an empty-payload segment (useful for creating empty ACK segments)
    void send_empty_segment();

    //! \brief create and send segments to fill as much of the window as possible
    void fill_window();

    //! \brief Notifies the TCPSender of the passage of time
    void tick(const size_t ms_since_last_tick);
    //!@}

    //! \name Accessors
    //!@{

    //! \brief How many sequence numbers are occupied by segments sent but not yet acknowledged?
    //! \note count is in "sequence space," i.e. SYN and FIN each count for one byte
    //! (see TCPSegment::length_in_sequence_space())
    size_t bytes_in_flight() const;

    //! \brief Number of consecutive retransmissions that have occurred in a row
    unsigned int consecutive_retransmissions() const;

    //! \brief TCPSegments that the TCPSender has enqueued for transmission.
    //! \note These must be dequeued and sent by the TCPConnection,
    //! which will need to fill in the fields that are set by the TCPReceiver
    //! (ackno and window size) before sending.
    std::queue<TCPSegment> &segments_out() { return _segments_out; }
    //!@}

    //! \name What is the next sequence number? (used for testing)
    //!@{

    //! \brief absolute seqno for the next byte to be sent
    uint64_t next_seqno_absolute() const { return _next_seqno; }

    //! \brief relative seqno for the next byte to be sent
    WrappingInt32 next_seqno() const { return wrap(_next_seqno, _isn); }
    //!@}
};

#endif  // SPONGE_LIBSPONGE_TCP_SENDER_HH
