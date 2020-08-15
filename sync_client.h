#include <boost/noncopyable.hpp>
#include <cstdint>
#include "sync_mess.pb.h"
#include "uv_client.h"
#include "decoder.h"

namespace sync_client
{
using namespace uv;
class SyncClient : public UVClient
{
public:
  SyncClient();

protected:
  int ping();
  virtual int do_on_connect(uv_connect_t* req, int status) override;
  virtual int do_after_write(uv_write_t* req, int status) override;
  virtual size_t do_init_write_req() override;
  virtual int do_on_close(uv_handle_t* handle) override;
  virtual int do_on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf);

private:
  reactor::Decoder<filesync::SyncPackage, int64_t> decoder_;
};

}
