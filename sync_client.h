#pragma once
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
  SYNCING = 0, PAUSED, FAILED, CANCELED, IDLE
};

struct SyncEntryInfo {
  SyncEntryInfo()
  {
    filename = nullptr;
    total_len = 0;
    sent = 0;
    target = 0;
    state = SyncEntryState::IDLE;
  }
  const std::string* filename;
  uint64_t total_len;
  uint64_t sent;
  uint64_t target;
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
  int start_send_file(const char* path);
  int file_cb(uv_fs_t* req, uv_fs_type fs_type);

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
  filesync::SyncPackagePtr hello_package_;
  char* client_hello_package_ = nullptr;
  int64_t client_hello_package_size_ = 0;
  uint64_t timer_interval_ = 0;
  bool is_ping_failed_ = false;
  bool is_write_buf_full_ = false;
  char* mes_ = nullptr;

  std::map<std::string, SyncEntryInfo> sync_entry_map_;
  std::map<std::string, FSFile*> fs_files_map_;
};

}
