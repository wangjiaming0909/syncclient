#include "uv_client.h"
#include "easylogging++.h"

namespace uv
{
UVClient::UVClient()
{
  memset(&server_addr_, 0, sizeof(struct sockaddr_in));
}

int UVClient::init(const char* server_addr, int port)
{
  if (uv_ip4_addr(server_addr, port, &server_addr_)) {
    LOG(ERROR) << "addr or port error: " << strerror(errno) << " , addr: " << server_addr << " port: " << port;
    return -1;
  }
  tcp_ = new uv_tcp_t();
  if (uv_tcp_init(uv_default_loop(), tcp_)) {
    LOG(ERROR) << "tcp init error: " << strerror(errno);
    return -1;
  }
  connect_req_ = new uv_connect_t();
  return 0;
}

void UVClient::on_connect(uv_connect_t* req, int status)
{

}
}
