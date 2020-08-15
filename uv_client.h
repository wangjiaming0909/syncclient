#include <boost/noncopyable.hpp>
#include <uv.h>
#include <string>
#include "buffer.h"

namespace uv
{
void connect_cb(uv_connect_t* req, int status);
void after_write_cb(uv_write_t* req, int status);
void close_cb(uv_handle_t* handle);
void read_alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf);
void read_cb(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf);

class UVClient : boost::noncopyable
{
public:
  friend void connect_cb(uv_connect_t* req, int status);
  friend void after_write_cb(uv_write_t* req, int status);
  friend void close_cb(uv_handle_t* handle);
  friend void read_alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf);
  friend void read_cb(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf);
  UVClient();
  ~UVClient();
  int init(const char*server_addr, int port);
  int start();
  int do_write();
  //buffer provide space for wirte
  int write(const char* d, size_t size, bool flush);
  template <typename T>
  int write(const T& d, bool flush)
  {
    auto ret = my_write_buf_.append(d);
    if (flush) {
      return do_write();
    }
    return ret;
  }

protected:
  int on_connect(uv_connect_t* req, int status);
  int after_write(uv_write_t* req, int status);
  int on_close(uv_handle_t* handle);
  int read_alloc(uv_handle_t* handle, size_t size, uv_buf_t* buf);
  int on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf);

  //return how many bytes initialized
  //if return 0 means that no data to write
  size_t init_write_req();

protected:
  virtual size_t do_init_write_req() = 0;
  virtual int do_on_connect(uv_connect_t* req, int status) = 0;
  virtual int do_after_write(uv_write_t* req, int status) = 0;
  virtual int do_on_close(uv_handle_t* handle) = 0;
  virtual int do_on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf) = 0;

protected:
  uv_tcp_t* tcp_;
  uv_connect_t* connect_req_;
  uv_write_t* write_req_;
  uv_buf_t write_buf_;
  reactor::buffer my_write_buf_;
  reactor::buffer my_read_buf_;
  struct sockaddr_in server_addr_;
  std::string server_addr_str_;
  int server_port_;
};
}
