#include "uv_client.h"
#include "easylogging++.h"

namespace uv
{
void connect_cb(uv_connect_t* req, int status)
{
  auto* client = static_cast<UVClient*>(req->handle->data);
  if(!client) {
    LOG(ERROR) << "req with a null client in connect_cb";
    return;
  }
  client->on_connect(req, status);
}

void after_write_cb(uv_write_t* req, int status)
{
  auto* client = static_cast<UVClient*>(req->handle->data);
  if(!client) {
    LOG(ERROR) << "req with a null client in after_connect_cb";
    return;
  }
  client->after_write(req, status);
}

void close_cb(uv_handle_t* handle)
{
  auto* client = static_cast<UVClient*>(handle->data);
  client->on_close(handle);
}

void read_alloc_cb(uv_handle_t* handle, size_t size, uv_buf_t* buf)
{
  buf->base = (char*)calloc(size, 1);
  buf->len = size;
}

void read_cb(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf)
{
  auto* client = (UVClient*)stream->data;
  client->on_read(stream, size, buf);
}

void timer_cb(uv_timer_t* handle)
{
  auto*client = (UVClient*)handle->data;
  if (client) {
    client->on_timeout(handle);
  }
}

UVClient::UVClient()
{
  memset(&server_addr_, 0, sizeof(struct sockaddr_in));
}
UVClient::~UVClient()
{
  delete tcp_;
  tcp_ = nullptr;
  delete connect_req_;
  connect_req_ = nullptr;
}

int UVClient::init(const char* server_addr, int port)
{
  server_addr_str_ = server_addr;
  server_port_ = port;
  if (uv_ip4_addr(server_addr, port, &server_addr_)) {
    LOG(ERROR) << "addr or port error: " << strerror(errno) << " , addr: " << server_addr << " port: " << port;
    return -1;
  }
  tcp_ = new uv_tcp_t();
  if (uv_tcp_init(get_loop(), tcp_)) {
    LOG(ERROR) << "tcp init error: " << strerror(errno);
    return -1;
  }
  tcp_->data = this;
  connect_req_ = new uv_connect_t();

  timer_ = new uv_timer_t();
  timer_->data = this;
  uv_timer_init(get_loop(), timer_);
  return 0;
}

int UVClient::start()
{
  if (uv_tcp_connect(connect_req_, tcp_, (const struct sockaddr*)&server_addr_, connect_cb)) {
    LOG(ERROR) << "connect error: " << strerror(errno) << " addr: " << server_addr_str_ << " port: " << server_port_;
  }
  int ret = 0;
  do{
    ret = uv_run(get_loop(), UV_RUN_DEFAULT);
  }while(ret == 0);
  return ret;
}

int UVClient::do_write()
{
  int ret = 0;
  if (init_write_req() > 0) {
    if ((ret = uv_write(write_req_, (uv_stream_t*)tcp_, &write_buf_, 1, after_write_cb))) {
      uv_close((uv_handle_t*)tcp_, close_cb);
    }
  }
  return ret;
}

int UVClient::write(const char* d, size_t size, bool flush)
{
  LOG(DEBUG) << "UVClient write";
  auto ret = my_write_buf_.append((const void*)d, size);
  if ( ret < 0) {
    LOG(ERROR) << "UVClient::write error";
    return 0;
  }
  if (flush)
    return do_write();
  else
    return ret;
}

int UVClient::start_timer(uint64_t timeout, uint64_t repeat)
{
  LOG(DEBUG) << "UVClient start_timer";
  if (timer_) {
    return uv_timer_start(timer_, timer_cb, timeout, repeat);
  }
  return -1;
}

int UVClient::stop_timer()
{
  LOG(DEBUG) << "UVClient stop_timer";
  if (timer_)
    return uv_timer_stop(timer_);
  return -1;
}

size_t UVClient::init_write_req()
{
  write_req_ = new uv_write_t();
  memset(&write_buf_, 0, sizeof(write_buf_));
  do_init_write_req();
  if (my_write_buf_.total_len() > 0) {
    auto p = my_write_buf_.pullup(my_write_buf_.total_len() > 4096 ? 4096 : my_write_buf_.total_len());
    //LOG(DEBUG) << "init_write_req len: " << my_write_buf_.first_chain_length();
    write_buf_ = uv_buf_init(p, my_write_buf_.first_chain_length());
  }
  return write_buf_.len;
}

int UVClient::on_connect(uv_connect_t* req, int status)
{
  LOG(DEBUG) << "UVClient on_connect";
  if (status < 0) {
    LOG(ERROR) << "connect error: " << strerror(-status);
    uv_close((uv_handle_t*)tcp_, close_cb);
    return -1;
  }
  LOG(DEBUG) << "on connect in syncclient status: " << status;
  uv_read_start((uv_stream_t*)req->handle, read_alloc_cb, read_cb);
  return do_on_connect(req, status);
}

int UVClient::after_write(uv_write_t* req, int status)
{
  my_write_buf_.drain(write_buf_.len);
  do_after_write(req, status);
  if (my_write_buf_.total_len() > 0) {
    do_write();
  }
  return 0;
}

int UVClient::on_close(uv_handle_t* handle)
{
  LOG(DEBUG) << "UVClient on_close";
  return do_on_close(handle);
}

int UVClient::on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf)
{
  LOG(DEBUG) << "UVClient on_read";
  if (size < 0) {
    LOG(INFO) << "got EOF";
    free(buf->base);
    uv_close((uv_handle_t*)tcp_, close_cb);
    return -1;
  }
  if (do_on_read(stream, size, buf) < 0) {
    uv_close((uv_handle_t*)tcp_, close_cb);
    return -1;
  }
  return 0;
}

int UVClient::on_timeout(uv_timer_t* handle)
{
  LOG(DEBUG) << "UVClient on_timeout";
  return do_on_timeout(handle);
}
}
