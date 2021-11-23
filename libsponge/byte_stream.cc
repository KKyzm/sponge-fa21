#include "byte_stream.hh"

// Dummy implementation of a flow-controlled in-memory byte stream.

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&.../* unused */) {}

using namespace std;

ByteStream::ByteStream(const size_t capacity)
    : _capacity(capacity), _end_input(false), _bytes_written(0), _bytes_read(0), _error(false) {
    // DUMMY_CODE(capacity);
}

size_t ByteStream::write(const string &data) {
    // DUMMY_CODE(data);
    if (remaining_capacity() == 0)
        return 0;

    size_t written_len = min(remaining_capacity(), data.size());
    _buf.insert(_buf.end(), data.begin(), data.begin() + written_len);
    _bytes_written += written_len;
    _capacity -= written_len;
    return written_len;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    // DUMMY_CODE(len);
    size_t read_len = min(buffer_size(), len);
    return string(_buf.begin(), _buf.begin() + read_len);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    // DUMMY_CODE(len);
    size_t pop_len = min(buffer_size(), len);
    _buf.erase(_buf.begin(), _buf.begin() + pop_len);
    _bytes_read += pop_len;
    _capacity += pop_len;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    // DUMMY_CODE(len);
    string read_bytes = peek_output(len);
    pop_output(len);
    return read_bytes;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buf.size(); }

bool ByteStream::buffer_empty() const { return buffer_size() == 0; }

bool ByteStream::eof() const { return _end_input && buffer_empty(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

// size_t ByteStream::remaining_capacity() const { return _capacity - buffer_size(); }
size_t ByteStream::remaining_capacity() const { return _capacity; }
