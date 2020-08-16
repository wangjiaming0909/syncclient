#include <boost/noncopyable.hpp>
#include <cstdint>
#include "sync_mess.pb.h"
#include "uv_client.h"
#include "decoder.h"
#include "sync_package.h"


namespace sync_client
{
using namespace uv;
using namespace filesync;
class SyncClient : public UVClient
{
static uint64_t DEFAULT_TIMER_INTERVAL;
public:
  SyncClient();
  ~SyncClient();

protected:
  int ping();
  virtual int do_on_connect(uv_connect_t* req, int status) override;
  virtual int do_after_write(uv_write_t* req, int status) override;
  virtual size_t do_init_write_req() override;
  virtual int do_on_close(uv_handle_t* handle) override;
  virtual int do_on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf) override;
  virtual int do_on_timeout(uv_timer_t* handle) override;

private:
  reactor::Decoder<filesync::SyncPackage, int64_t> decoder_;
  char* client_hello_package_ = nullptr;
  size_t client_hello_package_size_ = 0;
  uint64_t timer_interval_;
  bool is_ping_failed_ = false;
};

}
