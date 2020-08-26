#include "buffer.h"
#include <exception>
#include <utility>

using namespace reactor;

buffer_iter::buffer_iter( const buffer* buffer_ptr
    , const buffer_chain* chain
    , uint64_t offset_of_buffer
    , uint32_t chain_number
    , uint32_t offset_of_chain)
  : buffer_(buffer_ptr)
  , chain_(chain)
  , offset_of_buffer_(offset_of_buffer)
  , chain_number_(chain_number)
  , offset_of_chain_(offset_of_chain)
{

}

const buffer_iter buffer_iter::NULL_ITER = buffer_iter{nullptr, nullptr, 0, 0, 0};

buffer_iter& buffer_iter::operator+(uint32_t forward_steps)
{
  if(forward_steps == 0) return *this;

  uint32_t maximum_steps_can_forward = buffer_->total_len() - this->offset_of_buffer_;
  if(forward_steps > maximum_steps_can_forward) {
    chain_ = buffer_->last_chain_with_data();
    offset_of_chain_ = chain_->get_offset();
    offset_of_buffer_ = buffer_->total_len();
    chain_number_ = 0;//!
    return *this;
  }

  //    int forward_chain_numbers = -1;
  int64_t steps_can_forward_in_current_chain = chain_->get_offset() - this->offset_of_chain_;
  assert(steps_can_forward_in_current_chain >= 0 && "offset of a chain should > offset_of_chain in buffer_iter");
  uint32_t remain_steps = forward_steps;

  //current is not the last chain_with_data
  while(!buffer_->is_last_chain_with_data(chain_) && remain_steps > steps_can_forward_in_current_chain) {
    remain_steps -= steps_can_forward_in_current_chain;
    chain_ = chain_->next();
    steps_can_forward_in_current_chain = chain_->get_offset();
    chain_number_++;
    this->offset_of_chain_ = 0;
  }
  //last chain_with_data or current chain can cover the remain_steps
  //note that current chain could be the original chain(means that chain_ didn't changed at all)
  offset_of_chain_ = this->offset_of_chain_ + remain_steps;
  offset_of_buffer_ += forward_steps;
  return *this;
}

bool buffer_iter::operator>(const buffer_iter& other)
{
  //其中有任何一个buffer是0就不比较了，直接返回false
  if(buffer_ == nullptr || other.buffer_ == nullptr) return false;
  // 根本就不是同一个buffer的
  if(other.buffer_ != buffer_ ) return false;

  if(!buffer_->validate_iter(other) || !other.buffer_->validate_iter(*this)) return false;

  return offset_of_buffer_ > other.offset_of_buffer_;
}

bool buffer_iter::operator<(const buffer_iter& other)
{
  return !this->operator>(other);
}

bool buffer_iter::operator==(const buffer_iter& other)
{
  if(buffer_ == nullptr || other.buffer_ == nullptr) return false;
  if(other.buffer_ != buffer_ ) return false;
  if(!buffer_->validate_iter(other) || !other.buffer_->validate_iter(*this)) return false;
  return offset_of_buffer_ == other.offset_of_buffer_;
}

buffer_chain::buffer_chain(buffer* parent, uint32_t capacity)
  : buffer_(nullptr)
  , off_(0)
  , next_(nullptr)
  , parent_(parent)
    , misalign_(0)
{
  if(capacity == 0)
  {
    LOG(WARNING) << "capacity_ is 0";
    capacity_ = DEFAULT_CHAIN_SIZE;
  }
  else
  {
    capacity_ = this->calculate_actual_capacity(capacity);
  }

  // buffer_ = static_cast<void*>(new char[capacity_]);
  buffer_ = static_cast<char*>(::calloc(capacity_, 1));
  if(capacity_ > 4096)
  {
    //LOG(INFO) << "allocating memory in buffer chain size: " << capacity_ << " ------------------------------";
  }
  assert(buffer_ != nullptr && ("new operator error size"));
}

buffer_chain::~buffer_chain()
{
  if(buffer_ != nullptr)
    free(buffer_);
}

buffer_chain::buffer_chain(buffer_chain&& other)
{
  buffer_ = other.buffer_;
  capacity_ = other.capacity_;
  off_ = other.off_;
  next_ = other.next_;
  parent_ = other.parent_;
  misalign_ = other.misalign_;

  other.buffer_ = nullptr;
  other.capacity_ = 0;
  other.off_ = 0;
  other.next_ = nullptr;
  other.parent_ = nullptr;
  other.misalign_ = 0;
}

//// other 原来是什么样，复制后的对象也是什么样，capacity, misalign的大小都是一样的
buffer_chain::buffer_chain(const buffer_chain& other)
{
  capacity_ = calculate_actual_capacity(other.capacity_);
  buffer_ = static_cast<char*>(::calloc(capacity_, 1));
  assert(buffer_ != nullptr && ("new operator error size"));

  ::memcpy(buffer_, other.get_start_buffer(), other.size());
  misalign_ = 0;
  next_ = other.next_;
  off_ = other.size();
  parent_ = other.parent_;
}

buffer_chain::buffer_chain(const buffer_chain& other, uint32_t data_len, Iter start)
{
  //check if start is in the chain {other}, and within the range of other
  if( !other.validate_iter(start) || //TODO < or <=
      data_len > (other.off_ - start.offset_of_chain_)) //data_len太长, 超过other中已有的长度
  {
    throw std::exception();
  }

  //想要拷贝的字节数大于现有的字节数(现有是指从chain的最后到iter, 不是chain.size())
  // if(data_len > (other.off_ - start.offset_of_chain_))
  // {
  //     //TODO 貌似不正确
  //     ::new(this)buffer_chain{other}; return;
  // }

  ::new(this)buffer_chain(other.parent_, data_len);
  memcpy(buffer_, other.buffer_ + start.offset_of_chain_, data_len);
  next_ = other.next_;
  off_ = data_len;
  misalign_ = 0;
}

buffer_chain& buffer_chain::operator= (const buffer_chain& other)
{
  if(this == &other) return *this;

  if(capacity_ < other.capacity_)
  {
    if(buffer_ != nullptr)
      free(buffer_);
    capacity_ = other.capacity_;
    buffer_ = static_cast<char*>(::calloc(capacity_, 1));
  }

  ::memcpy(buffer_, other.buffer_ + other.misalign_, other.size());
  next_ = other.next_;
  off_ = other.size();
  parent_ = other.parent_;
  misalign_ = 0;
  return *this;
}

uint32_t buffer_chain::append(const buffer_chain& chain)
{
  uint32_t size = chain.size();//防止自己append给自己，先记下size
  if(size > chain_free_space()) return 0;
  ::memcpy(buffer_ + off_, chain.buffer_ + chain.misalign_, size);
  off_ += size;
  return size;
}

uint32_t buffer_chain::append(const buffer_chain& chain, uint64_t len, Iter start)
{
  if(len > chain.size() || !chain.validate_iter(start) || len > chain.off_ - start.offset_of_chain_)
  {
    return 0;
  }
  ::memcpy(buffer_ + off_, chain.buffer_ + start.offset_of_chain_, len);
  off_ += len;
  return len;
}

int64_t buffer_chain::append(const void* data, uint64_t data_len)
{
  if(data == 0 || data_len == 0) return -1;

  if(chain_free_space() < data_len) return -1;

  ::memcpy(buffer_ + off_, data, data_len);
  off_ += data_len;
  return data_len;
}

bool buffer_chain::validate_iter(Iter it) const
{
  if( it.chain_ != this ||
      it.offset_of_chain_ >= this->off_ ||
      it.offset_of_chain_ < this->misalign_)  //TODO < or <=
    return false;
  return true;
}

int buffer_chain::set_offset(uint32_t offset)
{
  if(offset > capacity_) return -1;
  off_ = offset; return 0;
}

buffer_chain::Iter buffer_chain::begin() const
{
  return Iter{this->parent_, this, this->parent_->iter_of_chain(*this).offset_of_buffer_, 0, misalign_};
}

buffer_chain::Iter buffer_chain::end() const
{
  return Iter{this->parent_, this, this->parent_->iter_of_chain(*this).offset_of_buffer_ + this->off_, 0, off_};
}

uint32_t buffer_chain::calculate_actual_capacity(uint32_t given_capacity)
{
  uint32_t to_alloc = 0;
  if(given_capacity < MAXIMUM_CHAIN_SIZE / 2) {
    to_alloc = DEFAULT_CHAIN_SIZE;
    while(to_alloc < given_capacity){
      to_alloc <<= 1;
    }
  } else {
#ifdef MINIMUM_CHAIN_SIZE
    given_capacity = given_capacity < MINIMUM_CHAIN_SIZE ? MINIMUM_CHAIN_SIZE : given_capacity;
#endif
    to_alloc = given_capacity;
  }
  return to_alloc;
}

buffer::buffer()
  : chains_()
  , last_chain_with_data_(nullptr)
    , total_len_(0)
{

}

buffer::buffer(const buffer& other) : chains_(), last_chain_with_data_(nullptr), total_len_(0)
{
  this->chains_ = other.chains_;
  update_last_chain_with_data(other);
  update_next_field_after_copy();
  this->total_len_ = other.total_len_;
}

buffer::buffer(const buffer& other, uint32_t data_len) : chains_(), last_chain_with_data_(nullptr), total_len_(0)
{
  if(other.total_len_ == 0 || data_len == 0)
  {
    ::new(this)buffer{}; return;
  }

  //copy all
  if(data_len >= other.total_len())
  {
    ::new(this)buffer{other}; return;
  }

  //partially copy
  uint32_t remain_to_copy = data_len;
  const buffer_chain* current_chain = &other.first();

  while(!other.is_last_chain_with_data(current_chain) && current_chain->size() < remain_to_copy)
  {
    // ASSERT_CHAIN_FULL(current_chain)// 并不一定是full了
    append(*current_chain);
    this->last_chain_with_data_ = &chains_.back();
    remain_to_copy -= current_chain->size();
    current_chain = current_chain->next();
  }

  //get to the last_chain_with_data or current_chain can cover remain_to_copy
  //even it is the last_chain_with_data, it can also cover the remain_to_copy
  buffer_chain last_chain{*current_chain, remain_to_copy, current_chain->begin()};
  append(last_chain);
  //update {next chain} of 倒数第二个 chain, 因为刚刚新添加一个chain
  if(last_chain_with_data_)
    last_chain_with_data_->set_next_chain(&chains_.back());
  last_chain_with_data_ = &chains_.back();
  total_len_ = data_len;
}

buffer::buffer(const buffer& other, uint32_t data_len, Iter start) : chains_(), last_chain_with_data_(nullptr), total_len_(0)
{
  if(!other.validate_iter(start) || data_len == 0 || other.total_len_ == 0)
  {
    ::new(this)buffer{}; return;
  }

  const buffer_chain* current_chain = start.chain_;
  if(!current_chain->validate_iter(start)) //validate start in current_chain iter, start must within misalign_ and off_
  {
    ::new(this)buffer{}; return;
  }

  uint32_t bytes_can_copy_in_current_chain = current_chain->off_ - start.offset_of_chain_;
  Iter start_iter_in_current_chain = start;
  uint32_t maximum_bytes_can_copy_out = other.total_len_ - start.offset_of_buffer_;
  uint32_t remain_to_copy = data_len > maximum_bytes_can_copy_out ? maximum_bytes_can_copy_out : data_len;
  bool start_from_begin = false;// if the start_iter has been used, so next time we need to read from front of next chain

  while(  !other.is_last_chain_with_data(current_chain) &&
      bytes_can_copy_in_current_chain < remain_to_copy)
  {
    buffer_chain _chain{*current_chain, bytes_can_copy_in_current_chain, start_iter_in_current_chain};
    append(_chain);

    this->last_chain_with_data_ = &chains_.back();

    remain_to_copy -= bytes_can_copy_in_current_chain;
    current_chain = current_chain->next();
    bytes_can_copy_in_current_chain = current_chain->size();
    start_iter_in_current_chain = current_chain->begin();
    start_from_begin = true;
  }

  //get to the last chain or current chain can cover {remain_to_copy}
  assert(current_chain != nullptr && "current should not be nullptr");

  if(start_from_begin)
    append(buffer_chain{*current_chain, remain_to_copy, current_chain->begin()});
  else
    append(buffer_chain{*current_chain, remain_to_copy, start_iter_in_current_chain});

  this->last_chain_with_data_ = &chains_.back();
  total_len_ = data_len;
}

buffer_chain* buffer::push_back(buffer_chain&& chain)
{
  //rvalue ref can't call lvalue ref
  // return push_back(chain);
  if(chains_.size() == 0)
  {
    chains_.push_back(std::move(chain));
  }
  else
  {
    auto& last_chain = chains_.back();
    chains_.push_back(std::move(chain));
    last_chain.next_ = &chains_.back();
    chains_.back().set_next_chain(nullptr);
  }
  return &chains_.back();
}

//push_back与data没有关系, 仅仅是向chains_中添加节点
buffer_chain* buffer::push_back(const buffer_chain& chain)
{
  if(chains_.size() == 0)
  {
    chains_.push_back(chain);
  }
  else
  {
    auto& last_chain = chains_.back();
    chains_.push_back(chain);
    last_chain.next_ = &chains_.back();
    chains_.back().set_next_chain(nullptr);
  }

  return &chains_.back();
}

//同样的, push_front并不关心数据的问题， 仅仅是向chains的头部中添加一个chain
buffer_chain* buffer::push_front(buffer_chain&& chain)
{
  buffer_chain* first_chain = chains_.size() == 0 ? nullptr : &chains_.front();
  chains_.push_front(std::move(chain));

  chains_.front().next_ = first_chain;
  return &chains_.front();
}

buffer_chain* buffer::push_front(buffer_chain& chain)
{
  buffer_chain* first_chain = chains_.size() == 0 ? nullptr : &chains_.front();
  chains_.push_front(chain);
  chains_.front().next_ = first_chain;
  return &chains_.front();
}

buffer::Iter buffer::begin()
{
  return Iter{this, &(this->chains_.front()), 0, 0, this->chains_.front().misalign_};
}

buffer::Iter buffer::end()
{
  return Iter{this, last_chain_with_data_, this->total_len_, 0, last_chain_with_data_->get_offset()};
}

buffer::Iter buffer::iter_of_chain(const buffer_chain& chain)
{
  if(total_len_ == 0) return Iter::NULL_ITER;
  uint32_t offset_of_buffer = 0;
  buffer_chain* current_chain = &this->chains_.front();

  while(current_chain != nullptr && current_chain != &chain)
  {
    // ASSERT_CHAIN_FULL(current_chain)
    offset_of_buffer += current_chain->chain_capacity();
    current_chain = current_chain->next();
  }

  //the chain is not found
  if(current_chain == nullptr)
  {
    return Iter::NULL_ITER;
  }
  return Iter{this, current_chain, offset_of_buffer, 0, current_chain->misalign_};
}

buffer& buffer::operator=(const buffer& other)
{
  if(this == &other) return *this;
  if(other.buffer_length() == 0)
    return *this;
  chains_ = other.chains_;
  total_len_ = other.total_len_;
  update_last_chain_with_data(other);
  return *this;
}

uint32_t buffer::first_chain_length()
{
  if(chains_.size() == 0 || chains_.front().get_offset() == 0) return 0;
  return chains_.front().size();
}

int64_t buffer::append(const buffer& other, uint64_t data_len, Iter start)
{
  if(!other.validate_iter(start) || other.total_len_ == 0 || data_len == 0)
  {
    return -1;
  }

  free_trailing_empty_chains();

  const buffer_chain *current_chain = start.chain_;
  if(!current_chain->validate_iter(start))
  {
    return -1;
  }

  uint32_t bytes_can_copy_in_current_chain = current_chain->off_ - start.offset_of_chain_;
  Iter start_iter_in_current_chain = start;
  uint32_t maximum_bytes_can_copy_out = other.total_len_ - start.offset_of_buffer_;
  uint32_t remain_to_copy = data_len > maximum_bytes_can_copy_out ? maximum_bytes_can_copy_out : data_len;
  uint32_t total_bytes_going_to_copy = remain_to_copy;
  bool start_from_begin = false; // if the start_iter has been used, so next time we need to read from front of next chain

  while(  !other.is_last_chain_with_data(current_chain) &&
      bytes_can_copy_in_current_chain < remain_to_copy)
  {
    buffer_chain _chain{*current_chain, bytes_can_copy_in_current_chain, start_iter_in_current_chain};
    append(_chain);

    this->last_chain_with_data_ = &chains_.back();
    //        total_len_ += bytes_can_copy_in_current_chain;

    remain_to_copy -= bytes_can_copy_in_current_chain;
    current_chain = current_chain->next();
    bytes_can_copy_in_current_chain = current_chain->size();
    start_iter_in_current_chain = current_chain->begin();
    start_from_begin = true;
  }

  //get to the last chain or current chain can cover {remain_to_copy}
  assert(current_chain != nullptr && "current should not be nullptr");

  if(start_from_begin)
  {
    auto _chain = buffer_chain{*current_chain, remain_to_copy, current_chain->begin()};
    append(_chain);
  }
  else
  {
    auto _chain = buffer_chain{*current_chain, remain_to_copy, start_iter_in_current_chain};
    append(_chain);
  }

  //    total_len_ += total_bytes_going_to_copy;
  return total_bytes_going_to_copy;
}

int64_t buffer::append(const buffer_chain &chain)//TODO copy too much
{
  uint32_t size = chain.size();
  auto* current_chain = expand_if_needed(size);
  assert(current_chain->chain_free_space() >= size);
  //TODO check if that current_chain is the same as last_chain_with_data
  //expand之后的current_chain就是之前的last_chain_with_data, 因此没有扩展
  if(last_chain_with_data_ == current_chain)//从之前的位置进行copy
  {
    ::memcpy(current_chain->buffer_ + last_chain_with_data_->off_, chain.get_start_buffer(), size);
  }
  else
  {//已经expand了,因此从头开始copy, 其中可能之前并没有任何数据,因此last_chain_with_data_ == 0
    assert(last_chain_with_data_ == nullptr || last_chain_with_data_->next_ == current_chain);
    ::memcpy(current_chain->buffer_, chain.get_start_buffer(), size);
  }

  current_chain->off_ += size;
  last_chain_with_data_ = current_chain;
  total_len_ += size;
  return size;
}

int64_t buffer::append(buffer_chain &&chain)
{
  if(last_chain_with_data_ != nullptr)
  {
    auto * last_chain = free_trailing_empty_chains();
    chains_.push_back(std::move(chain));
    last_chain->next_ = &chains_.back();
  }
  else
  {
    chains_.push_back(std::move(chain));
  }
  chains_.back().next_ = nullptr;
  chains_.back().parent_ = this;
  return chains_.back().size();//!!
}

int64_t buffer::append(const void* data, uint64_t data_len)
{
  if(data == 0 || data_len == 0) return -1;

  buffer_chain* data_chain = expand_if_needed(data_len);
  if(data_chain->chain_free_space() < data_len) return -1;//for expand error

  data_chain->append(data, data_len);
  total_len_ += data_len;
  last_chain_with_data_ = data_chain;
  return data_len;
}

int64_t buffer::append_printf(const char* fmt, ...)
{
  if(fmt == 0)     {
    LOG(WARNING) << "fmt error: " << fmt;
    return -1;
  }
  int ret = 0;
  buffer_chain* data_chain = expand_if_needed(64);
  char* data_p = data_chain->buffer_ + data_chain->off_;
  uint32_t data_size = data_chain->chain_free_space();

  for(;;)
  {
    va_list va;
    va_start(va, fmt);
    int vs = vsnprintf(data_p, data_size, fmt, va);
    va_end(va);

    if(vs < 0) return vs;//vsnprintf return error

    if(buffer_chain::MAXIMUM_CHAIN_SIZE < static_cast<uint32_t>(vs))//fmt 字串太长
    {
      LOG(WARNING) << "Too long for a chain, size: " << vs;
      return -1;
    }

    if((uint32_t)vs < data_size)//data_size可以塞下fmt字串
    {
      data_chain->off_ += static_cast<uint32_t>(vs);
      total_len_ += static_cast<uint32_t>(vs);
      ret = vs;
      last_chain_with_data_ = data_chain;
      break;
    }

    data_chain = expand_if_needed(static_cast<uint32_t>(vs) + 1);
    data_p = data_chain->buffer_ + data_chain->off_;
    data_size = data_chain->chain_free_space();
  }
  return ret;
}

int64_t buffer::append_vprintf(const char* fmt, va_list ap)
{
  (void)fmt;
  (void)ap;
  return 0;
}

int buffer::prepend(const buffer& other, uint32_t data_len, Iter start)
{
  (void)other;
  (void)data_len;
  (void)start;
  return 0;
}

char* buffer::pullup(int64_t size)
{
  if(size < 0) //传递负值,表示全部align到第一个节点
    size = total_len_;

  //如果size 比 total_len_ 大, 那么将不能保证第一个node可以达到size的大小,因此返回nullptr
  if(size == 0 || uint64_t(size) > total_len_)
    return nullptr;

  //LOG(DEBUG) << "pullup size: " << size << " total_len: " << total_len_;
  //LOG(DEBUG) << "chain size: " << chains_.size();
  buffer_chain* first_chain = &chains_.front();
  //如果第一个chain的大小已经满足size了,那么直接返回
  if(first_chain->size() >= size)
    return static_cast<char*>(first_chain->get_start_buffer());

  //第一个chain不够
  int64_t remain_to_pullup = size - first_chain->size();
  //LOG(DEBUG) << "remain to pullup: " << remain_to_pullup;

  //如果first_chain的free size可以塞下多出来的部分, 那么就可以不移动任何元素，直接拷贝多出来的部分
  if(first_chain->chain_free_space() >= remain_to_pullup) {
    //        buffer_chain& next_chain = *first_chain.next();
    //        ::memcpy(first_chain.off_, next_chain.buffer_ + next_chain.misalign_, remain_to_pullup);
    //        first_chain.off_ += remain_to_pullup;
    //        next_chain.misalign_ += remain_to_pullup;
  } else//并不能塞下多出来的部分,重新分配一个chain, 保证第一个chain size >= size
  {
    buffer_chain chain{this, static_cast<uint32_t>(size)};
    chain = *first_chain;//此处做了align, 设置了chain的next
    chains_.pop_front();
    chains_.push_front(std::move(chain));
  }

  first_chain = &chains_.front();
  buffer_chain *current_chain = first_chain->next();
  //LOG(DEBUG) << "first_chain size: " << first_chain->size();

  //current_chian可能是就是last_chain_with_data_, 但是current_chain不可能是空chain
  while( current_chain != nullptr && remain_to_pullup >= current_chain->size()) {
    assert(current_chain->size() > 0);
    //LOG(DEBUG) << "pullup from current chain size: " << current_chain->size();
    remain_to_pullup -= current_chain->size();
    //LOG(DEBUG) << "after pull once remain_to_pullup: " << remain_to_pullup;
    first_chain->append(*current_chain);
    first_chain->next_ = current_chain->next_;
    chains_.erase(++chains_.begin());
    current_chain = first_chain->next_;
  }

  //current_chain == 0 或者 只需要 current_chain 中的一部分
  if(remain_to_pullup != 0)//还有一部分需要拷贝
  {
    first_chain->append(*current_chain, static_cast<uint64_t>(remain_to_pullup), current_chain->begin());
    current_chain->misalign_ += remain_to_pullup;
  }

  last_chain_with_data_ = current_chain ? last_chain_with_data_ : first_chain;
  //LOG(DEBUG) << "after pull up first buffer_chain size: " << first_chain->size();
  return static_cast<char*>(first_chain->get_start_buffer());
}

int64_t buffer::remove(/*out*/void* data, uint32_t data_len)
{
  if(data == nullptr || data_len == 0) return -1;
  //根本就没有数据 or 数据不够
  if(last_chain_with_data_ == nullptr || total_len_ < data_len)  return -1;

  //数据够
  buffer_chain * current_chain = &chains_.front();
  buffer_chain* next_chain = current_chain->next();
  uint32_t remain_to_remove = data_len;
  uint32_t dest_start_pos = 0;

  for (; remain_to_remove > 0; current_chain = next_chain, next_chain = current_chain->next()) {
    if(current_chain->size() <= remain_to_remove)
    {
      //remove this chain
      assert(current_chain != nullptr);
      assert(current_chain->size() > 0);
      ::memcpy(static_cast<char*>(data) + dest_start_pos, current_chain->get_start_buffer(), current_chain->size());
      remain_to_remove -= current_chain->size();
      dest_start_pos += current_chain->size();
      //            assert(last_chain_with_data_ != current_chain);//不一定，可能当前chain就是最后一个有data 的 chain，remain_to_remove 就等于 size
      total_len_ -= current_chain->size();
      chains_.pop_front();

      //!!! ERROR 不应该判断next是不是空,而应该判断 当前 chian是不是last_chain_with_data
      if(/*next_chain == 0*/ current_chain == last_chain_with_data_)  //当当前chain已经是最后一个的时候,next就是空的,此时不需要再循环了,已经结束了
      {
        assert(remain_to_remove == 0);
        break;
      }
      continue;
    }

    //remain data to remove && 当前 remove  到 当前 chain 结束, 并且当前chain 不是全部remove
    ::memcpy(static_cast<char*>(data) + dest_start_pos, current_chain->get_start_buffer(), remain_to_remove);
    current_chain->misalign_ += remain_to_remove;
    total_len_ -= remain_to_remove;
    break;
  }
  return data_len;
}

//TODO remove the calloc and free
int64_t buffer::drain(uint32_t len)
{
  void* datap = ::calloc(len, 1);
  int64_t ret = remove(datap, len);
  free(datap);
  return ret;
}

int64_t buffer::copy_out_from(void* data, uint32_t data_len, Iter start)
{
  if(!validate_iter(start) || data == nullptr)
    return -1;
  if(data_len == 0) return 0;

  buffer tmp_buf{*this, data_len, start};//COPY ONCE
  return tmp_buf.remove(data, data_len);//COPY ONCE
}

int64_t buffer::read_line(char * read_out, uint32_t n, buffer_eol_style eol_style)
{
  if(read_out == nullptr || n == 0) return -1;

  uint32_t eol_len = 0;
  auto it = search_eol(&eol_len, eol_style, begin());
  if(!it.is_valid()) return -1;
  //如果得了n个字节，但是并没有读到换行符，那么就返回-1, 表示read_out不够存下一行的数据
  if(n < it.offset_of_buffer_) return -1;

  int64_t bytes_read = remove(read_out, it.offset_of_buffer_);
  drain(eol_len);
  return bytes_read;
}

buffer_iter buffer::search(const char* what, uint32_t len, Iter start)
{
  return search_range(what, len, start, end());
}

buffer_iter buffer::search_range(const char* what, uint32_t len, Iter start, Iter end)
{
  if(!validate_iter(start) || !validate_iter(end) || len == 0 || what == nullptr)
  {
    return Iter::NULL_ITER;
  }

  auto* first_chain = start.chain_;
  auto* last_chain = end.chain_;
  uint32_t off = start.offset_of_chain_;
  uint32_t last_off = start.offset_of_chain_;
  auto* current_chain = first_chain;
  auto* next_chain = current_chain->next_;
  const char first = what[0];
  void* target = nullptr;
  uint32_t target_offset_of_chain = 0;
  bool FOUND = false;

  for(;
      current_chain && current_chain->size() > 0;
      current_chain = next_chain, next_chain = current_chain ? current_chain->next_ : nullptr, off = current_chain ? current_chain->misalign_ : 0)
  {
    if(current_chain == last_chain) off = last_off;

    while(true)
    {
      target = ::memchr(current_chain->buffer_ + off, first, current_chain->off_ - off);
      if(target == nullptr) break;
      //in one chain, there could be some first char
      off = static_cast<char*>(target) - current_chain->buffer_ + 1;
      target_offset_of_chain = static_cast<char*>(target) - static_cast<const char*>(current_chain->get_start_buffer());
      //memcmp, 注意可能之后的字符再下一个chain中
      if(buffer_memcmp(what, len, iter_of_chain(*current_chain) + (target_offset_of_chain - current_chain->misalign_)))
      {
        FOUND = true;
        break;
      }
    }
    if(FOUND) break;
  }

  if(!FOUND)
  {
    return Iter::NULL_ITER;
  }

  return iter_of_chain(*current_chain) + target_offset_of_chain;
}

bool buffer::buffer_memcmp(const char* source, uint32_t len, Iter start)
{
  if(!validate_iter(start) || source == nullptr || len > total_len_ - start.offset_of_buffer_)
  {
    return false;
  }

  const buffer_chain* current_chain = start.chain_;
  uint32_t off = start.offset_of_chain_;
  const buffer_chain* next_chain = current_chain->next_;
  uint32_t remain_to_compare = len;
  uint32_t off_of_source = 0;

  while (current_chain && current_chain->size() > 0 && remain_to_compare > 0) {
    uint32_t size_going_to_compare = remain_to_compare > (current_chain->off_ - off) ? (current_chain->off_ - off) : remain_to_compare;
    if(::memcmp(current_chain->buffer_ + off, source + off_of_source, size_going_to_compare) != 0)
      return false;
    remain_to_compare -= size_going_to_compare;
    off_of_source += size_going_to_compare;

    if(next_chain == nullptr) break;
    current_chain = next_chain;
    next_chain = current_chain->next_;
    off = current_chain->misalign_;
  }

  if(remain_to_compare == 0)  return true;
  return false;
}

buffer_iter buffer::search_eol(uint32_t* eol_len_out, buffer_eol_style eol_style, Iter start)
{
  if(eol_len_out == nullptr || !validate_iter(start) || total_len_ == 0)
  {
    return Iter::NULL_ITER;
  }

  auto it_CRLF = search_range("\r\n", 2, begin(), end());
  auto it_LF = search_range("\n", 1, begin(), end());
  auto it_NUL = search_range("\0", 1, begin(), end());

  Iter it_ret = Iter::NULL_ITER;

  switch(eol_style)
  {
    case buffer_eol_style::BUFFER_EOL_CRLF:
      {
        //找\n, 也要找\r
        if(!it_CRLF.is_valid())
        {
          if(it_LF.is_valid())
          {
            it_ret = it_LF < it_CRLF ? it_LF : it_CRLF;//when it_LF == it_CRLF, return it_CRLF
            *eol_len_out = 2;
          }
        }
        it_ret = it_CRLF;
        *eol_len_out = 1;
        break;
      }
    case buffer_eol_style::BUFFER_EOL_CRLF_STRICT:
      {
        //找\r\n
        it_ret = it_CRLF;
        *eol_len_out = 2;
        break;
      }
    case buffer_eol_style::BUFFER_EOL_LF:
      //找\n
      it_ret = it_LF;
      *eol_len_out = 1;
      break;
    case buffer_eol_style::BUFFER_EOL_NUL:
      //找\0
      it_ret = it_NUL;
      *eol_len_out = 1;
      break;
  }

  return it_ret;
}

int buffer::peek(std::vector<const buffer_iovec*> vec_out, uint32_t len, Iter start)
{
  (void)vec_out;
  (void)len;
  (void)start;
  return 0;
}

inline bool buffer::is_last_chain_with_data(const buffer_chain* current_chain) const
{
  if(current_chain == nullptr || current_chain != last_chain_with_data_)
    return false;
  return true;
}

bool buffer::validate_iter(const Iter& iter) const
{
  //validate buffer
  if(iter.buffer_ != this)
  {
    return false;
  }

  //find the chain
  const buffer_chain* current_chain = &*this->chains_.begin();
  while(current_chain != nullptr && current_chain != iter.chain_)
  {
    current_chain = current_chain->next_;
  }

  //validate the chain
  if(current_chain == iter.chain_)
  {
    if( iter.chain_->off_ == current_chain->off_ &&
        iter.chain_->buffer_ == current_chain->buffer_ &&
        iter.chain_->misalign_ == current_chain->misalign_)
      return true;
  }
  assert(current_chain == nullptr && "any other conditions???");
  return false;
}

buffer_chain* buffer::expand_if_needed(uint32_t data_len)
{
  //LOG(DEBUG) << "expand if needed data_len: " << data_len;
  //no data in buffer at all
  //TODO ERROR last_chain_with_data_ == 0并不代表没有chain，如果有几个空的chain，就可以直接使用，不需要新分配内存
  if(/*last_chain_with_data_ == 0*/ total_len_ == 0) {
    //        free_trailing_empty_chains();//有可能出现buffer中只有空的chain, 此处是将其free 掉之后重新插入
    //!! TODO 可以更加高效一点, 检查是否有空的chain，有就检查大小，如果可以塞得下，就直接使用，否则插入新的
    //! //也可以直接在前面插入新的，不free掉之后的内存，当之后往其中插入的时候，可以重复使用这部分内存
    //        assert(chains_.empty() && "chains_ should be empty");
    push_front(buffer_chain{this, data_len});
    return &chains_.back();
  }

  //if {last_chain_with_data_} has enough space for {data_len} then do nothing
  if (last_chain_with_data_->chain_free_space() >= data_len) {
    return last_chain_with_data_;
  }

  //检查是否值得扩展
  buffer_chain* lc = last_chain_with_data_;
  if (lc->chain_free_space() < (lc->chain_capacity() / 8) || //(比例上)当前chain 的剩余空间 只有 capacity 的 1/8, resize 需要拷贝较多内存
      lc->get_offset() > buffer_chain::MAXIMUM_SIZE_WHEN_EXPAND ||  // (绝对值上)chain 中已经存了 4096 个字节了
      data_len >= (buffer_chain::MAXIMUM_CHAIN_SIZE - lc->get_offset())) //即便进行扩展，可能扩展之后的空闲内存还是塞不下 data_len
  {
    //因此不进行扩展，so 看lc->next 的吧
    if(lc->next() && lc->next()->chain_free_space() > data_len) {
      //存在, 并且有足够的空间塞下 data_len
      assert(lc->next()->get_offset() == 0 && "lc->next() should be empty");
      return lc->next();
    } else {
      //要么是根本不存在next, 要么是next 不够(虽然是空的)
      free_trailing_empty_chains();
      buffer_chain _chain{this, data_len};
      push_back(std::move(_chain));
    }
  } else {
    //now we can resize lc
    uint32_t length_needed = lc->get_offset() + data_len;
    buffer_chain chain_newed{this, length_needed};
    //! lc 一定是最后一个chain么?? 为什么pop_back()
    chain_newed = *lc;//copy from lc into chain_newed
    chains_.pop_back();
    //pop_back 之后，取得最后一个chain, 再将现在的最后一个chain 的next 设置为之后加入的新chain
    last_chain_with_data_ = push_back(chain_newed);
    // last_chain_with_data_ = &chains_.back();
  }
  return &chains_.back();
}

buffer_chain* buffer::free_trailing_empty_chains()
{
  buffer_chain* chain = last_chain_with_data_;
  //no data at all
  if(total_len_ == 0)
  {
    chains_.clear(); return nullptr;
  }
  assert(chain->get_offset() > 0);

  auto start_iter = chains_.begin();
  while(&*start_iter != chain)
  {
    ++start_iter;
  }

  auto back_iter = chains_.end();
  --back_iter;
  while(back_iter != start_iter) {
    chains_.pop_back();
    chains_.back().set_next_chain(nullptr);
    --back_iter;
  }
  return chain;
}

buffer_chain* buffer::update_last_chain_with_data(const buffer& other)
{
  auto start = chains_.begin();
  auto end = chains_.end();
  auto other_start = other.chains_.begin();
  auto other_end = other.chains_.end();
  for (;
      start != end &&
      other_start != other_end &&
      (&*other_start != other.last_chain_with_data_);
      start++, other_start++);
  last_chain_with_data_ = &*start;
  return last_chain_with_data_;
}

void buffer::update_next_field_after_copy()
{
  //当从一个chains_ 拷贝时, 每个chain 的next也都是拷贝的, 因此next field是不对的,
  //通过遍历一遍更新每个节点的next
  if(chains_.size() <= 1) return;
  buffer_chain* current_chain = &chains_.front();
  for(auto it = ++chains_.begin(); it != chains_.end(); it++)
  {
    current_chain->next_ = &*it;
    current_chain = &*it;
  }
  chains_.back().next_ = nullptr;
}
