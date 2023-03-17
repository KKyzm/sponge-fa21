#include "byte_stream.hh"

#include <algorithm>

// For Lab 0, please replace with a real implementation that passes the
// automated checks run by `make check_lab0`.

// You will need to add private members to the class declaration in `byte_stream.hh`

using namespace std;

ByteStream::ByteStream(const size_t capacity) : _capacity(capacity) {}

size_t ByteStream::write(const string &data) {
    if (input_ended()) 
        return 0;

    // _capacity - _buffer.size() bytes at most can be written
    size_t max_bytes_written = min(data.size(), remaining_capacity());
    _buffer.insert(_buffer.end(), data.begin(), data.begin() + max_bytes_written);
    _bytes_written += max_bytes_written;
    return max_bytes_written;
}

//! \param[in] len bytes will be copied from the output side of the buffer
string ByteStream::peek_output(const size_t len) const {
    size_t max_bytes_peek = min(len, _buffer.size());
    return string(_buffer.begin(), _buffer.begin() + max_bytes_peek);
}

//! \param[in] len bytes will be removed from the output side of the buffer
void ByteStream::pop_output(const size_t len) {
    size_t max_bytes_pop = min(len, _buffer.size());
    _buffer.erase(_buffer.begin(), _buffer.begin() + max_bytes_pop);
    _bytes_read += max_bytes_pop;
}

//! Read (i.e., copy and then pop) the next "len" bytes of the stream
//! \param[in] len bytes will be popped and returned
//! \returns a string
std::string ByteStream::read(const size_t len) {
    std::string str_bytes_read = peek_output(len);
    pop_output(len);
    return str_bytes_read;
}

void ByteStream::end_input() { _end_input = true; }

bool ByteStream::input_ended() const { return _end_input; }

size_t ByteStream::buffer_size() const { return _buffer.size(); }

bool ByteStream::buffer_empty() const { return _buffer.size() == 0; }

bool ByteStream::eof() const { return buffer_empty() && input_ended(); }

size_t ByteStream::bytes_written() const { return _bytes_written; }

size_t ByteStream::bytes_read() const { return _bytes_read; }

size_t ByteStream::remaining_capacity() const { return _capacity - _buffer.size(); }
