#pragma once
#include <uv.h>
#include <string>
#include "buffer.h"

namespace uv
{

void fs_callback(uv_fs_t* req);

class FSFile
{
public:
  friend void fs_callback(uv_fs_t* req);
  using Callback = std::function<void (uv_fs_t*, uv_fs_type)>;
  FSFile(uv_loop_t* loop, const char* file_name, Callback cb);
  ~FSFile();
  int open(int flags, int mode);
  int close();
  int read(uint32_t size, int64_t offset);
  int stat();
  int write();
  reactor::buffer& read_buf() {return read_buf_;}
  reactor::buffer& write_buf() {return write_buf_;}
  const std::string& file_name() const {return file_name_;}

protected:
  uv_loop_t* loop_ = nullptr;
  uv_fs_t* fs_ = nullptr;
  std::string file_name_;
#ifdef __linux__
  uv_os_fd_t handle_;
#else
  uv_file handle_;
#endif //UNIX

  uv_buf_t uv_read_buf_;
  uv_buf_t uv_write_buf_;
  reactor::buffer read_buf_;
  reactor::buffer write_buf_;
  Callback cb_;
};

}
