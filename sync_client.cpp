#include "sync_client.h"
#include "easylogging++.h"


namespace sync_client
{

SyncClient::SyncClient()
  : UVClient()
{
}

void SyncClient::on_connect(uv_connect_t* req, int status)
{

}
}
