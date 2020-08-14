#include <boost/noncopyable.hpp>
#include <cstdint>
#include "uv_client.h"

namespace sync_client
{
using namespace uv;
class SyncClient : public UVClient
{
public:
  SyncClient();

protected:
  virtual void on_connect(uv_connect_t* req, int status) override;

private:
};

}
