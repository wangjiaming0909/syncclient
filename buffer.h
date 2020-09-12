#ifndef _BUFFER_H_
#define _BUFFER_H_
#include <cstdarg>
#include <vector>
#include <cstdlib>
#include <cassert>
#include <cstring>
#include <iostream>
#include <list>
#include "easylogging++.h"



namespace reactor
{


enum class buffer_eol_style{
    BUFFER_EOL_LF, //'\n'
    BUFFER_EOL_CRLF_STRICT, //"\r\n"
    BUFFER_EOL_CRLF, //'\n' or "\r\n"
    BUFFER_EOL_NUL // ASCII NUL
};

class buffer;
class buffer_chain;

class buffer_iter{
  friend class buffer;
  friend class buffer_chain;
protected:
  buffer_iter(
      const buffer* buffer_ptr,
      const buffer_chain* chain,
      uint64_t offset_of_buffer,
      uint32_t chain_number,
      uint32_t offset_of_chain);

public:
  buffer_iter(const buffer_iter& other) = default;
  buffer_iter& operator=(const buffer_iter& other) = default;
  //the position from the start of buffer
  uint32_t offset() const {return offset_of_chain_;}
  const buffer* get_buffer() const {return buffer_;}

  //manipulates the {pos_}, success returns 0, error returns -1
  //! any rearranging of the buffer could invalidate all iter
  //TODO add operators for iter
  //{forward_step} can't be negative
  //如果向前forward 这么多之后，已经超出了整个buffer 现存的所有数据的iter, 返回最后的iter, 即back()
  buffer_iter& operator+(uint32_t forward_steps);
  const buffer_chain& chain() {return *chain_;}
  //不是同一个buffer返回 false
  //是同一个buffer，> 返回 true
  bool operator>(const buffer_iter& other);
  bool operator<(const buffer_iter& other);
  bool operator==(const buffer_iter& other);
  bool is_valid() const{return buffer_ != nullptr && chain_ != nullptr;}

  public:
  static const buffer_iter NULL_ITER;
  private:
  const buffer*               buffer_;
  const buffer_chain*         chain_;
  uint64_t                    offset_of_buffer_;
  uint32_t                    chain_number_;
  uint32_t                    offset_of_chain_;
};

struct buffer_iovec{
  void*         iov_base;
  uint32_t      iob_len;
};

class buffer_chain{
public:
    friend class buffer;
    using Iter = buffer_iter;
    buffer_chain(buffer* parent = nullptr, uint32_t capacity = DEFAULT_CHAIN_SIZE);
    ~buffer_chain();
    //copy data from other, align the data when copying, and the capacity will be the same as other
    buffer_chain(const buffer_chain& other);
    buffer_chain(const buffer_chain& other, uint32_t data_len, Iter start);
    buffer_chain(buffer_chain&& other);
    //* note that if(this->capacity_ > other.capacity_),
    //* this function only would expand the capacity_ won't shrink
    buffer_chain& operator= (const buffer_chain& other);
    buffer_chain& operator= (buffer_chain&& other);
    int set_offset(uint32_t offset);
    uint32_t get_offset() const {return off_;}
    uint32_t size() const { return off_ - misalign_; }
    uint32_t chain_free_space() const {return capacity_ - off_;}
    template <typename T>
    int64_t append(const T& data);
    int64_t append(const void* data, uint64_t data_len);
    //chain.size 必须要小于this 的free space
    uint32_t append(const buffer_chain& chain);
    uint32_t append(const buffer_chain& chain, uint64_t len, Iter start);

#ifdef TESTING
public:
    uint32_t get_misalign()const {return misalign_;}
    int64_t set_misalign(uint32_t misalign)
    {
        if(misalign < misalign_ || misalign > off_) return -1;
        misalign_ = misalign;
        return misalign_;
    }
#endif
public:
    uint32_t chain_capacity() const { return capacity_; }
    void* get_buffer() { return buffer_; }
    const void* get_buffer() const  { return buffer_; }
    void* get_start_buffer() {return static_cast<char*>(buffer_) + misalign_;}
    const void* get_start_buffer() const {return static_cast<char*>(buffer_) + misalign_;}
    void set_next_chain(buffer_chain* next) {next_ = next;}
    buffer_chain* next() { return next_; }
    const buffer_chain* next() const {return next_;}
    //begin will return the first byte in chain(beyond the misalign_)
    Iter begin() const ;
    Iter end() const ;
    bool validate_iter(Iter it) const ;
    bool operator==(const buffer_chain& other) const
    {
        return buffer_ == other.buffer_ &&
                    capacity_ == other.capacity_ &&
                    ::strncmp(static_cast<const char*>(buffer_), static_cast<const char*>(other.buffer_), capacity_) == 0 &&
                    off_ == other.off_ &&
                    next_ == other.next_ &&
                    parent_ == other.parent_ &&
                    misalign_ == other.misalign_;
    }

  private:
    // 内存分配策略: precondition(given_capacity > 0)
    //    如果capacity > MAXIMUM_CHAIN_SIZE / 2, 直接分配内存
    //    如果capacity < MAXIMUM_CHAIN_SIZE / 2, 那么以 1024 的2 的幂次倍递增
    uint32_t calculate_actual_capacity(uint32_t given_capacity);
public:
    static const uint32_t DEFAULT_CHAIN_SIZE = 1024;
    static const uint32_t MAXIMUM_CHAIN_SIZE = UINT32_MAX;
    static const uint32_t MAXIMUM_SIZE_WHEN_EXPAND = 4096;//指示，当expand一个chain的时候，如果这个chain已经有这么多字节了，那么就不再expand此chain
private:
    char*               buffer_;
    uint32_t            capacity_;
    uint32_t            off_;//offset into chain, the total number of bytes stored in the chain
    buffer_chain*       next_;
    buffer*             parent_;
    uint32_t            misalign_;
};

//** 1, lock or not lock

//! linked list of chains, every chain 的内部, (前)后可能存在空白部分
class buffer
{
public:
    using Iter = buffer_iter;
public:
    buffer();
    ~buffer() = default;
    buffer(const buffer& other);
    buffer(buffer&& other);
    //copy {data_len} data to {this} from {other}
    buffer(const buffer& other, uint32_t data_len);
    //copy {data_len} data to {this} from {start} to {start + data_len} in {other}
    buffer(const buffer& other, uint32_t data_len, Iter start);
    buffer& operator=(const buffer& other);

public:
    //* return number of bytes stored in the buffer
    uint64_t buffer_length() const {return total_len_;}
    //* return number of bytes stored in the first chunk
    uint32_t first_chain_length();

    //begin and end 只是第一个个最后一个的iter, (除去misalign)
    Iter begin();
    //end 不是 STL 中的最后一个的下一个, 只是最后一个
    Iter end();
    Iter iter_of_chain(const buffer_chain& chain);

    //* add the data to the end of the buffer
    //will change the total_len_
    template <typename T>
    int64_t append(const T& data);
    // int append(const buffer& other, uint32_t data_len);
    //append {data_len} bytes from other, start from {start}
    int64_t append(const buffer& other, uint64_t data_len, Iter start);
    //append a whole chain into the buffer
    //it could resize the last_chain_with_data due to the memory allocation strategy
    //will change the total_len_
    int64_t append(const buffer_chain &chain);
    int64_t append(buffer_chain &&chain);
    int64_t append(const void* data, uint64_t data_len);
    int64_t append_printf(const char *fmt, ...);
    int64_t append_vprintf(const char* fmt, va_list ap);

    template <typename T>
    int prepend(const T& data);
    // int prepend(const buffer& other, uint32_t data_len);
    //prepend {data_len} bytes from other, start from {start}
    int prepend(const buffer& other, uint32_t data_len, Iter start);


    //"linearizes" the first size bytes of this, to ensure that they are all contiguous and occupying the same chunk of memory
    //if size is negative, the function lineratizes the entire buffer
    //if size is greater than the number of bytes in the buffer, the function returns null
    //otherwise pullup returns the first byte in the buffer
    //* note that if the size is the same as first_chunk_length will do nothing
    char* pullup(int64_t size);

    //remove the first datalen bytes to the {data}
    //if total length is small than data_len, all data will be copied
    //error returns -1, success will return bytes that copied
    int64_t remove(/*out*/void* data, uint32_t data_len);
    //behave the same as remove but do not return the removed data, just remove the first {len} bytes
    int64_t drain(uint32_t len);
    int64_t copy_out_from(void* data, uint32_t data_len, Iter start);
    int64_t read_line(char * read_out, uint32_t n, buffer_eol_style eol_style);
    template <typename T>
    int32_t read_T(T& t_out, uint32_t);

    //search
    buffer_iter search(const char* what, uint32_t len, Iter start);
    buffer_iter search_range(const char* what, uint32_t len, Iter start, Iter end);
    //detect line_endings as read_line, but do not copy out the line, returns a iter to the start of the end-of-line character(s)
    //if {eol_len_out} is non-NULL, it is set to the length of the EOL string
    buffer_iter search_eol(uint32_t* eol_len_out, buffer_eol_style eol_style, Iter start);
    bool buffer_memcmp(const char* source, uint32_t  len, Iter start);


    //inspecting data without cpoying, returns bytes that returned
    int peek(std::vector<const buffer_iovec*> vec_out, uint32_t len, Iter start);

    const buffer_chain* last_chain_with_data() const { return last_chain_with_data_; }
    bool is_last_chain_with_data(const buffer_chain* current_chain) const;
    uint64_t total_len() const { return total_len_; }
    uint32_t chain_number() const {return this->chains_.size();}
    bool validate_iter(const Iter& iter) const ;

private:
    buffer_chain* push_back(buffer_chain&& chain);
    buffer_chain* push_back(const buffer_chain& chain);
    buffer_chain* push_front(buffer_chain&& chain);
    buffer_chain* push_front(buffer_chain& chain);
    //validate {iter}, if {iter} is in current {chain_}, return true, otherwise return false
    buffer_chain& first() { return chains_.front(); }
    const buffer_chain& first() const { return chains_.front();}
    buffer_chain& last() { return chains_.back(); }
    const buffer_chain& last() const { return chains_.back(); }
    //alters the last chunk of the memory in the buffer,
    //or add a chunk so that the buffer is large enough to add data_len bytes without any allocation
    //if {last_chain_with_data} has enough space for data_len, return directly
    //if not enough, expand it:
	//? 1, 把当前的最后一个chunk resize 一下, 使其后面没有空余空间, 然后直接 往最后 添加(插入, 因此可能之后还有空的chunk)一个 chunk就行
	//? 2, 直接 add一个新的chunk (之前的chunk之后的可用空间就浪费了)
	//? 3, 将当前最后一个chunk中数据考出来, new 一个chunk(大小是之前的off + datalen),把考出来的数据, 考进去
    buffer_chain* expand_if_needed(uint32_t data_len);
    buffer_chain* free_trailing_empty_chains();
    buffer_chain* update_last_chain_with_data(const buffer& other);
    void update_next_field_after_copy();
private:
    // bi-direactional linked list
    std::list<buffer_chain>           chains_;
    buffer_chain*                     last_chain_with_data_;//最后一个有数据的chain
    uint64_t                          total_len_;

#ifdef TESTING
public:
  const std::list<buffer_chain> &get_chains() const { return this->chains_; }
#endif
};

template <typename T>
int64_t buffer_chain::append(const T& data)
{
    if(this->chain_free_space() < sizeof(data)) return -1;
    ::memcpy(static_cast<char*>(buffer_) + this->off_, &data, sizeof(T));
    this->off_ += sizeof(T);
    return sizeof(T);
}

template <typename T>
int64_t buffer::append(const T& data)
{
    uint32_t size_needed = sizeof(T);
    auto chain = expand_if_needed(size_needed);
    if(chain == nullptr) return -1;
    chain->append(data);
    this->total_len_ += size_needed;
    last_chain_with_data_ = chain;
    return size_needed;
}

template <typename T>
int32_t buffer::read_T(T& t_out, uint32_t size)
{
  auto* d = pullup(size);
  if (d == nullptr) return -1;
  memcpy(&t_out, d, size);
  return size;
}

template <typename T>
int buffer::prepend(const T& data)
{
    return 0;
}

}//namespace reactor

#endif // _BUFFER_H_
