#include <boost/noncopyable.hpp>
#include <cstdint>
#include "sync_mess.pb.h"
#include "uv_client.h"
#include "decoder.h"
#include "sync_package.h"

#ifndef SYNC_PREFIX
#define SYNC_PREFIX "sync_"
#endif

namespace sync_client
{
using namespace uv;
using namespace filesync;

enum class SyncEntryState
{
  SYNCING = 0, PAUSED, FAILED, CANCELED
};

struct SyncEntryInfo {
  std::string* filename;
  uint64_t total_len;
  uint64_t sent;
  SyncEntryState state;
};

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
  virtual int do_on_fs_event(uv_fs_event_t* handle, const char* filename, int events, int status) override;

private:
  bool is_should_sync(const std::string& filename);

private:
  reactor::Decoder<filesync::SyncPackage, int64_t> decoder_;
  char* client_hello_package_ = nullptr;
  size_t client_hello_package_size_ = 0;
  uint64_t timer_interval_ = 0;
  bool is_ping_failed_ = false;

  std::map<std::string, SyncEntryInfo> sync_entry_map_;
};

}
