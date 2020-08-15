#include "sync_client.h"
#include "buffer.h"
#include "easylogging++.h"
#include <cstdlib>
#include <functional>
#include "sync_mess.pb.h"
#include "sync_package.h"


namespace sync_client
{

SyncClient::SyncClient()
  : UVClient()
{
}

int SyncClient::do_on_connect(uv_connect_t* req, int status)
{
  ping();
}

int SyncClient::ping()
{
  auto p = filesync::getHelloPackage("hello", filesync::PackageType::Client);
  auto size = p->ByteSizeLong();
  char* d = (char*)::calloc(size, 1);
  p->SerializeToArray(d, size);
  //TODO size type long?
  write(size, false);
  write(d, size, true);
  free(d);
  return size;
}

int SyncClient::do_after_write(uv_write_t* req, int status)
{
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
}
