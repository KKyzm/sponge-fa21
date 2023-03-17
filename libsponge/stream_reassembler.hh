#ifndef SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
#define SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH

#include "byte_stream.hh"

#include <cstdint>
#include <map>
#include <string>

//! \brief A class that assembles a series of excerpts from a byte stream (possibly out of order,
//! possibly overlapping) into an in-order byte stream.
class StreamReassembler {
  private:
    // map construct with default Compare = std::less<int>
    // which means the elements will be sorted in ascending order by the value of the key
    std::map<size_t, std::string> _reassembler{};
    bool _eof = false;                  //!< Whether EOF flag is set
    size_t _eof_index{};                //!< The index of EOF
    size_t _capacity_for_reassmbler{};  //!< The number of bytes reassembler could hold
    size_t _num_bytes_free{};           //!< The number of bytes could receive from the outside
    size_t _next_index = 0;             //!< The next index of byte to be pushed into _output

    ByteStream _output;      //!< The reassembled in-order byte stream
    const size_t _capacity;  //!< The maximum number of bytes

    //! \brief Receive a substring and write any newly contiguous bytes into the reassembler.
    void fit_in_reassembler(std::string &data, size_t index);

    //! \brief Receive a substring and its index, insert them into reassembler.
    size_t insert_in_reassembler(std::string data, size_t index);

    //! \brief Push bytes in reassembler into stream if could.
    void push_into_output();

    //! \brief Receive a substring and Return the part between specified index.
    std::pair<std::string, size_t> resize_segment(std::string &data, size_t idx, size_t b_idx, size_t e_idx);

    //! \brief Update the information about free space, like _capacity_for_reassmbler and _num_bytes_free.
    void update_free_space();

  public:
    //! \brief Construct a `StreamReassembler` that will store up to `capacity` bytes.
    //! \note This capacity limits both the bytes that have been reassembled,
    //! and those that have not yet been reassembled.
    StreamReassembler(const size_t capacity);

    //! \brief Receive a substring and write any newly contiguous bytes into the stream.
    //!
    //! The StreamReassembler will stay within the memory limits of the `capacity`.
    //! Bytes that would exceed the capacity are silently discarded.
    //!
    //! \param data the substring
    //! \param index indicates the index (place in sequence) of the first byte in `data`
    //! \param eof the last byte of `data` will be the last byte in the entire stream
    void push_substring(const std::string &data, const uint64_t index, const bool eof);

    //! \name Access the reassembled byte stream
    //!@{
    const ByteStream &stream_out() const { return _output; }
    ByteStream &stream_out() { return _output; }
    //!@}

    //! The number of bytes in the substrings stored but not yet reassembled
    //!
    //! \note If the byte at a particular index has been pushed more than once, it
    //! should only be counted once for the purpose of this function.
    size_t unassembled_bytes() const;

    //! \brief Is the internal state empty (other than the output stream)?
    //! \returns `true` if no substrings are waiting to be assembled
    bool empty() const;

    //! \brief The index of next byte that should be pushed into output.
    size_t get_next_index() const { return _next_index; }
};

#endif  // SPONGE_LIBSPONGE_STREAM_REASSEMBLER_HH
