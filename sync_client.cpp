#include "sync_client.h"
#include "easylogging++.h"
#include <functional>


namespace sync_client
{

SyncClient::SyncClient()
  : UVClient()
{
}

void SyncClient::on_connect(uv_connect_t* req, int status)
{
  if (status < 0) {
    LOG(ERROR) << "connect error: " << strerror(-status);
    return;
  }
  LOG(DEBUG) << "on connect in syncclient status: " << status;
}
}
