#include "stream_reassembler.hh"

#include <algorithm>
#include <utility>

// For Lab 1, please replace with a real implementation that passes the
// automated checks run by `make check_lab1`.

// You will need to add private members to the class declaration in `stream_reassembler.hh`

using namespace std;

StreamReassembler::StreamReassembler(const size_t capacity)
    : _capacity_for_reassmbler(capacity), _num_bytes_free(capacity), _output(capacity), _capacity(capacity) {}

//! \details This function accepts a substring (aka a segment) of bytes,
//! possibly out-of-order, from the logical stream, and assembles any newly
//! contiguous substrings and writes them into the output stream in order.
void StreamReassembler::push_substring(const string &data, const size_t index, const bool eof) {
    if (eof == true) {
        _eof = true;
        _eof_index = index + data.length();
    }

    update_free_space();
    if (_num_bytes_free == 0)
        return;

    string data_copy = data;

    fit_in_reassembler(data_copy, index);

    push_into_output();
}

void StreamReassembler::fit_in_reassembler(string &data, size_t index) {
    if (_num_bytes_free == 0)
        return;

    size_t next_index = _next_index;
    // firstly data should be resized to fit the free space of reassembler
    auto data_idx_pre = resize_segment(data, index, next_index, next_index + _capacity_for_reassmbler);
    data = data_idx_pre.first;
    index = data_idx_pre.second;
    size_t index_data_end = index + data.length();

    if (data.length() == 0) {
        return;
    }

    // if reassembler is empty
    if (empty() == true) {
        insert_in_reassembler(data, index);
        return;
    }

    for (auto iter : _reassembler) {
        // return if no free space
        if (_num_bytes_free == 0)
            return;

        auto data_idx_slot = resize_segment(data, index, next_index, iter.first);
        insert_in_reassembler(data_idx_slot.first, data_idx_slot.second);

        // update relevant variable to prepare for the next loop
        next_index = iter.first + iter.second.length();

        if (next_index >= index_data_end)
            break;
    }

    if (_num_bytes_free == 0)
        return;

    //
    auto riter = _reassembler.rbegin();
    size_t last_index = (*riter).first + (*riter).second.length();
    auto data_idx_end = resize_segment(data, index, last_index, index + data.length());
    insert_in_reassembler(data_idx_end.first, data_idx_end.second);
}

size_t StreamReassembler::insert_in_reassembler(string data, size_t index) {
    if (data.length() == 0)
        return 0;
    size_t size_insert = min(_num_bytes_free, data.length());
    string seg = data.substr(0, size_insert);
    _reassembler.insert(make_pair(index, seg));

    _num_bytes_free -= size_insert;
    return size_insert;
}

void StreamReassembler::push_into_output() {
    for (auto iter = _reassembler.begin(); iter != _reassembler.end(); iter = _reassembler.begin()) {
        if (_next_index == (*iter).first) {
            stream_out().write((*iter).second);
            _next_index += (*iter).second.length();
            _num_bytes_free += (*iter).second.length();
            _reassembler.erase(iter);
        } else
            break;
    }

    if (unassembled_bytes() == 0 && _eof == true && _next_index == _eof_index) {
        stream_out().end_input();
    }
}

pair<string, size_t> StreamReassembler::resize_segment(string &data, size_t index, size_t begin_idx, size_t end_idx) {
    // to make sure @begin_index is between [index, index + data.length()]
    // and @end_index is between [begin_index, index + data.length()]
    begin_idx = max(index, begin_idx);
    begin_idx = min(index + data.length(), begin_idx);

    end_idx = min(index + data.length(), end_idx);
    end_idx = max(begin_idx, end_idx);

    return make_pair(data.substr(begin_idx - index, end_idx - begin_idx), begin_idx);
}

void StreamReassembler::update_free_space() {
    _capacity_for_reassmbler = stream_out().remaining_capacity();
    _num_bytes_free = stream_out().remaining_capacity() - unassembled_bytes();
}

size_t StreamReassembler::unassembled_bytes() const {
    size_t num_bytes_unassembled = 0;
    for (auto iter = _reassembler.begin(); iter != _reassembler.end(); iter++) {
        num_bytes_unassembled += (*iter).second.length();
    }
    return num_bytes_unassembled;
}

bool StreamReassembler::empty() const { return _reassembler.size() == 0; }
