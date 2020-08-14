#include "uv_client.h"
#include "easylogging++.h"

namespace uv
{
void connect_callback(uv_connect_t* req, int status)
{
  auto* client = static_cast<UVClient*>(req->handle->data);
  if(!client) {
    LOG(ERROR) << "req with a null client";
    return;
  }
  client->on_connect(req, status);
}

UVClient::UVClient()
{
  memset(&server_addr_, 0, sizeof(struct sockaddr_in));
}
UVClient::~UVClient()
{
  delete tcp_;
  tcp_ = nullptr;
  delete connect_req_;
  connect_req_ = nullptr;
}

int UVClient::init(const char* server_addr, int port)
{
  server_addr_str_ = server_addr;
  server_port_ = port;
  if (uv_ip4_addr(server_addr, port, &server_addr_)) {
    LOG(ERROR) << "addr or port error: " << strerror(errno) << " , addr: " << server_addr << " port: " << port;
    return -1;
  }
  tcp_ = new uv_tcp_t();
  if (uv_tcp_init(uv_default_loop(), tcp_)) {
    LOG(ERROR) << "tcp init error: " << strerror(errno);
    return -1;
  }
  tcp_->data = this;
  connect_req_ = new uv_connect_t();
  return 0;
}

int UVClient::start()
{
  if (uv_tcp_connect(connect_req_, tcp_, (const struct sockaddr*)&server_addr_, connect_callback)) {
    LOG(ERROR) << "connect error: " << strerror(errno) << " addr: " << server_addr_str_ << " port: " << server_port_;
  }
  //LOG(INFO) << "connect to: " << server_addr_str_ << " succeed port: " << server_port_ << " succeed";
  return uv_run(uv_default_loop(), UV_RUN_DEFAULT);
}

void UVClient::on_connect(uv_connect_t* req, int status)
{
}
}
