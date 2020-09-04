#include "uv_client.h"
#include "buffer.h"
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
  (void)handle;
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

void prepare_cb(uv_prepare_t* handle)
{
  auto *client = (UVClient*)handle->data;
  client->on_prepare(handle);
}

void check_cb(uv_check_t* handle) {
  auto* client = (UVClient*)handle->data;
  client->on_check(handle);
}

void fs_event_cb(uv_fs_event_t* handle, const char* filename, int events, int status)
{
  auto* client = (UVClient*)handle->data;
  client->on_fs_event(handle, filename, events, status);
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
  delete ping_timer_;
  ping_timer_ = nullptr;
  delete write_req_;
  write_req_ = nullptr;
  delete reconnect_timer_;
  reconnect_timer_ = nullptr;
  for(auto* t : timers_) {
    delete t;
  }
  std::list<std::string> filenames{};
  for (auto it = fs_monitoring_map_.begin(); it != fs_monitoring_map_.end(); ++it) {
    filenames.push_back(it->first);
  }
  for (auto it = filenames.begin(); it != filenames.end(); ++it) {
    stop_fs_monitoring(*it);
  }
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
  connect_req_ = new uv_connect_t();

  ping_timer_ = new uv_timer_t();
  return 0;
}

int UVClient::start()
{
  if (connect_tcp() != 0) return -1;
  return uv_run(get_loop(), UV_RUN_DEFAULT);
}

int UVClient::do_write()
{
  int ret = 0;
  if (uv_is_closing((const uv_handle_t*)tcp_)) {
    LOG(WARNING) << "UVClient detects server is closing";
    return -1;
  }
  if (init_write_req() > 0) {
    if ((ret = uv_write(write_req_, (uv_stream_t*)tcp_, &write_buf_, 1, after_write_cb))) {
      close_tcp();
    }
  }
  return ret;
}

int UVClient::write(const char* d, size_t size, bool flush)
{
  //LOG(DEBUG) << "UVClient write";
  if (is_closed_) {
    LOG(ERROR) << "can't write now tcp is closed...";
    return -1;
  }
  if (my_write_buf_.total_len() > 1024 * 1024 * 10) {
    LOG(WARNING) << "writing too much please wait...";
    return 0;
  }
  auto ret = my_write_buf_.append((const void*)d, size);
  if ( ret < 0) {
    LOG(ERROR) << "UVClient::write error";
    return -1;
  }
  if (flush)
    return do_write();
  else
    return ret;
}

void UVClient::close_tcp()
{
  uv_close((uv_handle_t*)tcp_, close_cb);
}

int UVClient::connect_tcp()
{
  if (!tcp_) {
    tcp_ = new uv_tcp_t();
  } else {
    memset(tcp_, 0, sizeof(uv_tcp_t));
  }
  if (uv_tcp_init(get_loop(), tcp_)) {
    LOG(ERROR) << "tcp init error: " << strerror(errno);
    return -1;
  }
  tcp_->data = this;
  if (uv_tcp_connect(connect_req_, tcp_, (const struct sockaddr*)&server_addr_, connect_cb)) {
    LOG(ERROR) << "connect error: " << strerror(errno) << " addr: " << server_addr_str_ << " port: " << server_port_;
    return -1;
  }
  return 0;
}

int UVClient::start_reconnect_timer()
{
  if (!reconnect_timer_) {
    reconnect_timer_ = new uv_timer_t();
    uv_timer_init(get_loop(), reconnect_timer_);
    reconnect_timer_->data = this;
  }
  return uv_timer_start(reconnect_timer_, timer_cb, reconnect_fail_wait_, reconnect_fail_wait_);
}

void UVClient::wakeup_ping_timer()
{
  if (ping_timer_) start_ping_timer(0, 0);
}

void UVClient::close_loop()
{
  uv_stop(get_loop());
}

int UVClient::start_ping_timer(uint64_t timeout, uint64_t repeat)
{
  //LOG(DEBUG) << "UVClient start_ping_timer timeout: " << timeout << " repeat: " << repeat;
  if (ping_timer_) {
    uv_timer_stop(ping_timer_);
    ping_timer_->data = this;
    uv_timer_init(get_loop(), ping_timer_);
    return uv_timer_start(ping_timer_, timer_cb, timeout, repeat);
  }
  return -1;
}

int UVClient::stop_ping_timer()
{
  //LOG(DEBUG) << "UVClient stop_timer";
  if (ping_timer_)
    return uv_timer_stop(ping_timer_);
  return -1;
}

uv_timer_t* UVClient::start_timer(uv_timer_t* timer/*in out*/, uint64_t timeout, uint64_t repeat)
{
  if (timer == nullptr) {
    timer = new uv_timer_t();
    uv_timer_init(get_loop(), timer);
    timer->data = this;
    timers_.insert(timer);
  }
  if (uv_timer_start(timer, timer_cb, timeout, repeat) != 0)
    timer = nullptr;
  return timer;
}

int UVClient::stop_timer(uv_timer_t* timer)
{
  auto it = timers_.find(timer);
  int ret = 0;
  if (it == timers_.end()) {
    LOG(WARNING) << "stoping a nonexist timer: " << timer;
    ret = -1;
  }
  if(uv_timer_stop(timer) == 0) {
    timers_.erase(it);
    delete timer;
    timer = nullptr;
    ret = 0;
  } else {
    LOG(WARNING) << "stop timer: " << timer << " failed: " << strerror(errno);
    ret = -1;
  }
  return ret;
}

uv_check_t* UVClient::start_check(uv_check_t* check, std::function<int(uv_check_t*)> cb)
{
  if (check == nullptr) {
    check = new uv_check_t();
    uv_check_init(get_loop(), check);
    check->data = this;
    uv_check_start(check, check_cb);
  }
  auto p = check_cbs_.insert({check, cb});
  p.first->second = cb;
  return check;
}
int UVClient::stop_check(uv_check_t* check)
{
  auto it = check_cbs_.find(check);
  if (it == check_cbs_.end()) {
    LOG(DEBUG) << "can not stop nonexist check: " << check;
    return -1;
  }
  delete it->first;
  check_cbs_.erase(it);
  return 0;
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

int UVClient::start_fs_monitoring(const std::string& path_or_file)
{
  if (fs_monitoring_map_.count(path_or_file)) {
    LOG(WARNING) << "already monitoring file: " << path_or_file;
    return -1;
  }
  auto* fs_event = new uv_fs_event_t();
  uv_fs_event_init(get_loop(), fs_event);
  fs_event->data = this;
  if (uv_fs_event_start(fs_event, fs_event_cb, path_or_file.c_str(), UV_FS_EVENT_RECURSIVE) != 0) {
    delete fs_event;
    fs_event = nullptr;
    return -1;
  }
  fs_monitoring_map_[path_or_file] = fs_event;
  fs_event_map_[fs_event];
  if (fs_event_check_ == nullptr) {
    using namespace std::placeholders;
    fs_event_check_ = start_check(nullptr, std::bind(&UVClient::fs_event_check_cb, this, _1));
  }
  return 0;
}

void UVClient::stop_fs_monitoring(const std::string& path_or_file)
{
  auto it_monitoring = fs_monitoring_map_.find(path_or_file);
  if (it_monitoring != fs_monitoring_map_.end()) {
    auto* handle = it_monitoring->second;
    uv_fs_event_stop(handle);
    auto it = fs_event_map_.find(handle);
    if (it != fs_event_map_.end()) {
      for(auto info_it = it->second.begin(); info_it != it->second.end(); ++it) {
        delete info_it->second;
      }
      fs_event_map_.erase(it);
    }
    delete handle;
    fs_monitoring_map_.erase(it_monitoring);
  }
}

int UVClient::on_connect(uv_connect_t* req, int status)
{
  //LOG(DEBUG) << "UVClient on_connect";
  if (status < 0) {
    LOG(ERROR) << "connect error: " << strerror(-status);
    close_tcp();
    return -1;
  }
  //connect succeed
  if (is_should_reconnect_ && is_closed_) {
    is_closed_ = false;
    current_reconnect_retry_time_ = 0;
    wakeup_ping_timer();
  }
  //LOG(DEBUG) << "on connect in syncclient status: " << status;
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
  //LOG(DEBUG) << "UVClient on_close";
  is_closed_ = true;
  my_read_buf_.drain(my_read_buf_.total_len());
  my_write_buf_.drain(my_write_buf_.total_len());
  if (is_should_reconnect_ && current_reconnect_retry_time_ < reconnect_retry_times_) {
    if (start_reconnect_timer()) {
      LOG(ERROR) << "on close start reconnect timer failed";
      close_loop();
    }
  } else if (is_should_reconnect_) {
    LOG(ERROR) << "reconnect failed too many times closing loop";
    close_loop();
  }
  return do_on_close(handle);
}

int UVClient::on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf)
{
  //LOG(DEBUG) << "UVClient on_read";
  int ret = 0;
  if (size < 0 || do_on_read(stream, size, buf) < 0) {
    LOG(ERROR) << "got EOF or read error";
    close_tcp();
    ret = -1;
  }
  free(buf->base);
  return ret;
}

int UVClient::on_timeout(uv_timer_t* handle)
{
  //LOG(DEBUG) << "UVClient on_timeout";
  //ping timer to provide ping callback
  if (handle == ping_timer_)
    return do_on_timeout(handle);

  //base self timers
  if (handle == reconnect_timer_) {
    current_reconnect_retry_time_++;
    if (connect_tcp() != 0) {
      LOG(ERROR) << "on timeout reconnect tcp failed";
      close_loop();
      return -1;
    }
    uv_timer_stop(reconnect_timer_);
  }
  return 0;
}

int UVClient::on_prepare(uv_prepare_t* handle)
{
  (void)handle;
  int ret = 0;
  return ret;
}

int UVClient::on_check(uv_check_t* handle)
{
  auto it = check_cbs_.find(handle);
  if (it == check_cbs_.end()) {
    LOG(WARNING) << "can not find the check handle: " << handle;
    return -1;
  }
  return it->second(handle);
}

int UVClient::on_fs_event(uv_fs_event_t* handle, const char* filename, int events, int status)
{
  auto it = fs_event_map_.find(handle);
  auto t = uv_now(get_loop());
  //means this is the first time that this handle trigger a fs event
  if (it != fs_event_map_.end()) {
    auto it_ev_info = fs_event_map_[handle].find(filename);
    if (it_ev_info == fs_event_map_[handle].end()) {
      fs_event_info* fse_info = new fs_event_info{handle, filename, events, status, t};
      fs_event_map_[handle][filename] = fse_info;
      fse_info->last_timeout = t;
    } else {
      it_ev_info->second->last_timeout = t;
    }
  } else { //not the first time
    LOG(WARNING) << "fs event and fs_event_map not match handle: " << handle << " filename: " << filename << " events: " << events;
  }
  return 0;
}

int UVClient::fs_event_check_cb(uv_check_t* check)
{
  (void)check;
  auto now = uv_now(get_loop());
  decltype(fs_event_map_) m;
  for(auto& p : fs_event_map_) {
    for (auto it = p.second.begin(); it != p.second.end();) {
      if (now - it->second->last_timeout > fs_event_trigger_gap_) {
        m[p.first][it->first] = it->second;
        p.second.erase(it++);
      } else it++;
    }
  }
  for(auto& p : m) {
    for(auto it = p.second.begin(); it != p.second.end(); ++it) {
      do_on_fs_event(p.first, it->first.c_str(), it->second->events, it->second->status);
      delete it->second;
    }
  }
  return 0;
}
}
