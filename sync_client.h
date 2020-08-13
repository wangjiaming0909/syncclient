#include <uv.h>
#include <boost/noncopyable.hpp>
namespace sync_client
{

class SyncClient : boost::noncopyable
{
public:
  SyncClient();

  int start();

};

}
