#include <boost/noncopyable.hpp>
#include <uv.h>

namespace uv
{
class UVClient : boost::noncopyable
{
public:
  UVClient();
  int init(const char*server_addr, int port);
  int start();

protected:
  virtual void on_connect(uv_connect_t* req, int status);

private:
  uv_tcp_t* tcp_;
  uv_connect_t* connect_req_;
  uv_buf_t* buf_;
  struct sockaddr_in server_addr_;

};
}
