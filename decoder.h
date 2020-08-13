#pragma once
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <memory>
#include "buffer.h"

namespace reactor
{
template <typename T, typename Len_T>
class Decoder
{
public:
  enum DecodeState
  {
    IDLE = 0,
    LEN_DECODED,
    PARTIALY_PARSED,
    COMPLETED,
    ERROR
  };
public:
  using MessPtr = std::shared_ptr<T>;
  Decoder() : bytesParsed_{0}, bytesRemainToParse_{0}, state_{IDLE}, messLen_{0}, mesPtr_{nullptr} { }
  ~Decoder() { }
  Len_T decode(buffer& buf)
  {
    if (state_ == COMPLETED)
      reset();
    Len_T bytesParsed = 0;
    auto buf_len = buf.total_len();
    while(buf_len - bytesParsed > 0 && state_ != COMPLETED)
    {
      switch (state_)
      {
        case IDLE:
          if (buf_len < sizeof(Len_T))
            break;
          bytesParsed = decodeLen(buf);
          if (bytesParsed == sizeof(Len_T))
            state_ = LEN_DECODED;
          break;
        case LEN_DECODED:
        case PARTIALY_PARSED:
          {
            if (buf_len - bytesParsed < bytesRemainToParse_)
              return bytesParsed;
            bytesParsed += decodeRemain(buf);
            if(bytesRemainToParse_ == 0)
              state_ = COMPLETED;
            break;
          }
        case COMPLETED:
          break;
        case ERROR:
          return 0;
        default:
          break;
      }
    }
    return bytesParsed;
  }

  Len_T decode(const char* d, Len_T len)
  {
    Len_T bytesParsed = 0;
    while(len - bytesParsed > 0 && state_ != COMPLETED)
    {
      switch (state_)
      {
        case IDLE:
          if (len < sizeof(Len_T))
            break;
          bytesParsed += decodeLen(d, len);
          if (bytesParsed == sizeof(Len_T))
          {
            state_ = LEN_DECODED;
          }
          break;
        case LEN_DECODED:
        case PARTIALY_PARSED:
          {
            if (len - bytesParsed < bytesRemainToParse_) return bytesParsed;
            bytesParsed += decodeRemain(d + bytesParsed, len - bytesParsed);
            if (bytesRemainToParse_ == 0) state_ = COMPLETED;
            break;
          }
        case COMPLETED:
          break;
        case ERROR:
          return 0;
        default:
          break;
      }
    }
    return bytesParsed;
  }

  bool isCompleted() const { return state_ == COMPLETED; }

  bool isError() const {return state_ == ERROR;}

  MessPtr getMess() { return mesPtr_; }

  void reset()
  {
    LOG(DEBUG) << "Reset decoder...";
    mesPtr_.reset();
    bytesParsed_ = 0;
    bytesRemainToParse_ = 0;
    state_ = IDLE;
    messLen_ = 0;
  }

private:
  Len_T decodeLen(buffer& buf)
  {
    auto buf_len = buf.total_len();
    if (buf_len < sizeof(Len_T)) return 0;

    auto ret = buf.read_T(messLen_, sizeof(Len_T));
    if (ret != sizeof(Len_T))
    {
      messLen_ = 0;
      return 0;
    }
    LOG(DEBUG) << "Decoding len: " << messLen_;
    buf.drain(sizeof(Len_T));
    bytesParsed_ += sizeof(Len_T);
    bytesRemainToParse_ = messLen_;
    return sizeof(Len_T);
  }

  Len_T decodeRemain(buffer& buf)
  {
    auto buf_len = buf.total_len();
    if (buf_len < bytesRemainToParse_) return 0;

    assert(messLen_ == bytesRemainToParse_);
    auto* d = buf.pullup(messLen_);
    if (!d) return 0;
    mesPtr_ = std::make_shared<T>();
    auto ret = mesPtr_->ParseFromArray(d, messLen_);
    if (!ret)
    {
      state_ = ERROR;
      reset();
      return 0;
    }
    LOG(DEBUG) << "Decode remain mess: " << messLen_;
    bytesParsed_ += messLen_;
    LOG(DEBUG) << "bytes parsed: " << bytesParsed_;
    bytesRemainToParse_ -= messLen_;
    LOG(DEBUG) << "bytes remain to parse: " << bytesRemainToParse_;
    buf.drain(messLen_);
    return messLen_;
  }

private:
  Len_T bytesParsed_;
  Len_T bytesRemainToParse_;
  DecodeState state_;
  Len_T messLen_;
  MessPtr mesPtr_;
};

}
