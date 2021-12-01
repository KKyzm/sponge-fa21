#include "stream_reassembler.hh"

#include <utility>

// Dummy implementation of a stream reassembler.

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

template <typename... Targs>
void DUMMY_CODE(Targs &&... /* unused */) {}

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _first_unread (0)
    , _first_unassembled (0)
    , _first_unacquired (0)
    , _first_unacceptable (capacity)
    , _index_of_eof (0)
    , _eof (false)
    , _output (capacity)
    , _capacity (capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    _eof |= eof;
    if (eof)
        _index_of_eof = index + data.size();

    if (data.size() == 0) {
        if (_eof && index == _first_unassembled)
            _output.end_input();
        return;
    }

    _first_unread = _first_unassembled + _output.remaining_capacity() - _capacity;
    _first_unacceptable = _first_unread + _capacity;
    size_t real_head = 0;
    size_t real_end = 0;
    map<size_t, size_t> data_pieces{};

    if (index + data.size() <= _first_unassembled)
        return;
    else {
        size_t gap_head;
        size_t gap_end;
        gap_head = _first_unassembled;
        for (auto iter = _buf.begin(); iter != _buf.end(); gap_head = (iter++->second.get_endIndex())) {
            gap_end = iter->first;
            if ((index + data.size()) <= gap_head)  // 重复或越界
                break;
            if (index >= gap_end)
                continue;

            real_head = max(gap_head, index);
            real_end = min(gap_end, index + data.size());
            data_pieces.insert(make_pair(real_head, real_end));
        }
        if (index + data.size() > _first_unacquired) {
            real_head = max(_first_unacquired, index);
            real_end = min(index + data.size(), _first_unread + _capacity);
            data_pieces.insert(make_pair(real_head, real_end));
        }
    }

    for (auto iter_of_pieces = data_pieces.begin(); iter_of_pieces != data_pieces.end(); iter_of_pieces++) {
        real_head = iter_of_pieces->first;
        real_end = iter_of_pieces->second;
        string data_to_push = data.substr(real_head - index, real_end - real_head);
        _buf.insert(make_pair(real_head, StreamReassembler::sub_string(real_head, data_to_push)));
        _first_unacquired = max(_first_unacquired, real_end);
    }

    auto iter = _buf.begin();
    while (iter != _buf.end()) {
        if (iter->first == _first_unassembled) {
            _output.write(iter->second.get_string());
            _first_unassembled = iter->second.get_endIndex();

            if (_eof && (iter->second.get_endIndex() == _index_of_eof)) {
                _output.end_input();
                return;
            } else {
                iter = _buf.erase(iter);
                continue;
            }
        } else
            break;
    }
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t bytes_unasseml = 0;
    for (auto iter = _buf.cbegin(); iter != _buf.cend(); iter++) {
        bytes_unasseml += ((iter->second).get_string()).size();
    }
    return bytes_unasseml;
}

bool StreamReassembler::empty() const { return unassembled_bytes() == 0; }
