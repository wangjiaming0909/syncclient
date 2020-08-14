#include <boost/noncopyable.hpp>
#include <uv.h>
#include <string>

namespace uv
{
void connect_callback(uv_connect_t* req, int status);

class UVClient : boost::noncopyable
{
public:
  friend void connect_callback(uv_connect_t* req, int status);
  UVClient();
  ~UVClient();
  int init(const char*server_addr, int port);
  int start();

protected:
  virtual void on_connect(uv_connect_t* req, int status);

private:
  uv_tcp_t* tcp_;
  uv_connect_t* connect_req_;
  uv_buf_t* buf_;
  struct sockaddr_in server_addr_;
  std::string server_addr_str_;
  int server_port_;
};
}
