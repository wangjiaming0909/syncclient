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
void timer_cb(uv_timer_t* handle);
void prepare_cb(uv_prepare_t* handle);
void fs_event_cb(uv_fs_event_t* handle, const char* filename, int events, int status);

class UVClient : boost::noncopyable
{
public:
  friend void connect_cb(uv_connect_t* req, int status);
  friend void after_write_cb(uv_write_t* req, int status);
  friend void close_cb(uv_handle_t* handle);
  friend void read_alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf);
  friend void read_cb(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf);
  friend void timer_cb(uv_timer_t* handle);
  friend void prepare_cb(uv_prepare_t* handle);
  friend void fs_event_cb(uv_fs_event_t* handle, const char* filename, int events, int status);
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
    if (is_closed_) {
      LOG(ERROR) << "can't write now tcp is closed...";
      return -1;
    }
    auto ret = my_write_buf_.append(d);
    if (flush) {
      return do_write();
    }
    return ret;
  }

  uv_loop_t* get_loop() { return uv_default_loop();}
  void close_loop();
  int start_timer(uint64_t timeout, uint64_t repeat);
  int stop_timer();

  inline void set_should_reconnect(bool v) { is_should_reconnect_ = v; }
  int start_fs_monitoring(const std::string& path_or_file);
  void stop_fs_monitoring(const std::string& path_or_file);

protected:
  int on_connect(uv_connect_t* req, int status);
  int after_write(uv_write_t* req, int status);
  int on_close(uv_handle_t* handle);
  int read_alloc(uv_handle_t* handle, size_t size, uv_buf_t* buf);
  int on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf);
  int on_timeout(uv_timer_t* handle);
  int on_prepare(uv_prepare_t* handle);
  int on_fs_event(uv_fs_event_t* handle, const char* filename, int events, int status);

  //return how many bytes initialized
  //if return 0 means that no data to write
  size_t init_write_req();

protected:
  virtual size_t do_init_write_req() = 0;
  virtual int do_on_connect(uv_connect_t* req, int status) = 0;
  virtual int do_after_write(uv_write_t* req, int status) = 0;
  virtual int do_on_close(uv_handle_t* handle) = 0;
  virtual int do_on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf) = 0;
  virtual int do_on_timeout(uv_timer_t* handle) = 0;
  virtual int do_on_fs_event(uv_fs_event_t* handle, const char* filename, int events, int status) = 0;

private:
  void close_tcp();
  int connect_tcp();
  //prepare for reconnect, reinit the tcp
  int start_reconnect_timer();
  void wakeup_first_timer();

protected:
  uv_tcp_t* tcp_ = nullptr;
  uv_connect_t* connect_req_ = nullptr;
  uv_write_t* write_req_ = nullptr;
  uv_buf_t write_buf_;
  uv_timer_t* timer_ = nullptr;
  uv_timer_t* reconnect_timer_ = nullptr;
  reactor::buffer my_write_buf_;
  reactor::buffer my_read_buf_;
  struct sockaddr_in server_addr_;
  std::string server_addr_str_;
  int server_port_;
  bool is_closed_ = false;
  bool is_should_reconnect_ = false;
  int current_reconnect_retry_time_ = 0;
  std::map<std::string, uv_fs_event_t*> fs_monitoring_map_;
  static const int reconnect_fail_wait_ = 2000;//1 second
  static const int reconnect_retry_times_ = 5;
};
}
