#include "sync_client.h"
#include "buffer.h"
#include "easylogging++.h"
#include <cstdlib>
#include <functional>
#include "sync_mess.pb.h"


namespace sync_client
{
uint64_t SyncClient::DEFAULT_TIMER_INTERVAL = 200;

SyncClient::SyncClient()
  : UVClient()
{
  timer_interval_ = DEFAULT_TIMER_INTERVAL;
}
SyncClient::~SyncClient()
{
  delete client_hello_package_;
  client_hello_package_ = nullptr;
}

int SyncClient::do_on_connect(uv_connect_t* req, int status)
{
  LOG(DEBUG) << "SyncClient do_on_connect";
  return start_timer(timer_interval_, timer_interval_);
}

int SyncClient::ping()
{
  if (!client_hello_package_) {
    auto p = filesync::getHelloPackage("hello", filesync::PackageType::Client);
    client_hello_package_size_ = p->ByteSizeLong();
    client_hello_package_ = (char*)::calloc(client_hello_package_size_, 1);
    p->SerializeToArray(client_hello_package_, client_hello_package_size_);
  }
  //TODO size type long?
  write(client_hello_package_size_, false);
  return write(client_hello_package_, client_hello_package_size_, true);
}

int SyncClient::do_after_write(uv_write_t* req, int status)
{
  return 0;
}

size_t SyncClient::do_init_write_req()
{
  return 0;
}

int SyncClient::do_on_close(uv_handle_t* handle)
{
  return 0;
}

int SyncClient::do_on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf)
{
  using namespace filesync;
  reactor::buffer mb;
  mb.append(buf->base, size);
  while(mb.buffer_length() > sizeof(int64_t)) {
    auto len_parsed = decoder_.decode(mb);
    if (len_parsed <= 0) {
      if (decoder_.isError()) {
        LOG(ERROR) << "do on read parse error";
        return -1;
      }
    }
    if (decoder_.isCompleted()) {
      auto mess = decoder_.getMess();
      if (mess) {
        if (mess->header().command() == Command::ServerHello) {
          LOG(DEBUG) << "received server hello";
        }
      }
    }
    decoder_.reset();
  }
  return 0;
}

int SyncClient::do_on_timeout(uv_timer_t* handle)
{
  LOG(DEBUG) << "SyncClient do_on_timeout";
  auto ret = ping();
  if (ret < 0) {
    LOG(WARNING) << "ping server failed";
    is_ping_failed_ = true;
    timer_interval_ *= 2;
    start_timer(timer_interval_, timer_interval_);
  }
  if (ret >= 0 && is_ping_failed_) {
    is_ping_failed_ = false;
    timer_interval_ = DEFAULT_TIMER_INTERVAL;
    start_timer(timer_interval_, timer_interval_);
  }
  return ret;
}
}
