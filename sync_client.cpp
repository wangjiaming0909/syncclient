#include "sync_client.h"
#include "easylogging++.h"
#include "sync_mess.pb.h"
#include "boost/filesystem/path.hpp"
#include <boost/filesystem/operations.hpp>


namespace sync_client
{
uint64_t SyncClient::DEFAULT_TIMER_INTERVAL = 1000;

SyncClient::SyncClient()
  : UVClient()
{
  timer_interval_ = DEFAULT_TIMER_INTERVAL;
  is_should_reconnect_ = true;
  mes_ = (char*)::calloc(1, 4096+1024);
}
SyncClient::~SyncClient()
{
  delete client_hello_package_;
  client_hello_package_ = nullptr;
  free(mes_);
  for(auto& p : fs_files_map_)
    delete p.second;
}

int SyncClient::do_on_connect(uv_connect_t* req, int status)
{
  (void)req;
  (void)status;
  LOG(DEBUG) << "SyncClient do_on_connect";
  if (is_ping_failed_) {
    return 0;
  }
  return start_ping_timer(timer_interval_, timer_interval_);
}

//return 0 means write_buf is full
//return -1 means error
//return 1 means write succeed
int SyncClient::ping()
{
  if (!client_hello_package_) {
    hello_package_ = filesync::getHelloPackage("hello", filesync::PackageType::Client);
    client_hello_package_size_ = hello_package_->ByteSizeLong();
    client_hello_package_ = (char*)::calloc(client_hello_package_size_, 1);
    hello_package_->SerializeToArray(client_hello_package_, client_hello_package_size_);
  }
  //TODO size type long?
  int ret = 0;
  for(int i = 0; i < 1; i++) {
    LOG(INFO) << "pinging...";
    if ((ret = write(client_hello_package_size_, false, client_hello_package_size_ + 64)) > 0)
      write(client_hello_package_, client_hello_package_size_, true, 0);
    else if (ret < 0) return -1;
    else if (ret == 0){
      return 0;
    }
  }
  return 1;
}

int SyncClient::do_after_write(uv_write_t* req, int status)
{
  (void)req;
  (void)status;
  if (is_wrote_too_much_ && !check_is_writing_too_much()) {
    is_wrote_too_much_ = false;
    if (fses_.size() > 0) {
      auto it = fses_.erase(fses_.begin());
      file_cb(*it, UV_FS_READ);
    }
  }
  return 0;
}

size_t SyncClient::do_init_write_req()
{
  return 0;
}

int SyncClient::do_on_close(uv_handle_t* handle)
{
  (void)handle;
  return 0;
}

int SyncClient::do_on_read(uv_stream_t* stream, ssize_t size, const uv_buf_t* buf)
{
  (void)stream;
  using namespace filesync;
  //LOG(DEBUG) << "do on read size: " << size;
  //LOG(DEBUG) << "my_read_buf_ size: " << my_read_buf_.total_len();
  my_read_buf_.append(buf->base, size);
  while(my_read_buf_.buffer_length() > sizeof(int64_t)) {
    //LOG(DEBUG) << "before decode buf len: " << my_read_buf_.total_len();
    auto len_parsed = decoder_.decode(my_read_buf_);
    //LOG(DEBUG) << "after decode len parsed: " << len_parsed;
    //LOG(DEBUG) << "after decode buf len: " << my_read_buf_.total_len();
    if (len_parsed <= 0) {
      if (decoder_.isError()) {
        LOG(ERROR) << "do on read parse error";
        return -1;
      }
    }
    if (decoder_.isCompleted()) {
      auto mess = decoder_.getMess();
      if (mess) {
        if (mess->header().command() == Command::ServerHello) {
          LOG(INFO) << "received server hello";
        }
      }
    }
    if (len_parsed == 0)
      break;
  }
  //LOG(DEBUG) << "returning do on read buf len: " << my_read_buf_.total_len();
  return 0;
}

int SyncClient::do_on_timeout(uv_timer_t* handle)
{
  (void)handle;
  //LOG(DEBUG) << "SyncClient do_on_timeout";
  auto ret = ping();
  if (ret < 0) {
    LOG(WARNING) << "ping server failed";
    is_ping_failed_ = true;
    timer_interval_ *= 2;
    start_ping_timer(timer_interval_, timer_interval_);
  }
  if (ret == 1 && is_ping_failed_) {
    is_ping_failed_ = false;
    timer_interval_ = DEFAULT_TIMER_INTERVAL;
    start_ping_timer(timer_interval_, timer_interval_);
  }
  if (ret == 1 && is_write_buf_full_) {
    is_write_buf_full_ = false;
    timer_interval_ = DEFAULT_TIMER_INTERVAL;
    start_ping_timer(timer_interval_, timer_interval_);
  }
  if (ret == 0) {
    LOG(WARNING) << "ping write buf full";
    is_write_buf_full_ = true;
    timer_interval_ *= 2;
    start_ping_timer(timer_interval_, timer_interval_);
  }
  return ret;
}

bool SyncClient::is_should_sync(const boost::filesystem::path& p)
{
  if (p.filename().string().find(SYNC_PREFIX) != 0
      || !boost::filesystem::exists(p))
    return false;
  return true;
}

int SyncClient::do_on_fs_event(uv_fs_event_t* handle, const char* filename, int events, int status)
{
  (void)handle;
  LOG(DEBUG) << "do on fs event filename: " << filename << " events: " << events << " status: " << status;
  auto it = sync_entry_map_.find(filename);
  if (it != sync_entry_map_.end()) {
    LOG(INFO) << "cancel a syncing entry: " << filename << " and resyncing it";

  }
  auto absolute_path = boost::filesystem::path(handle->path).append(filename);
  if (!is_should_sync(absolute_path)) {
    LOG(INFO) << "skip sync file: " << filename;
  } else {
    LOG(INFO) << "start syncing: " << filename;
    fs_files_path_map_.insert({absolute_path.string(), filename});
    start_send_file(absolute_path.string().c_str());
  }
  return 0;
}

int SyncClient::start_send_file(const char* path, uint64_t offset, uint64_t target)
{
  auto p = sync_entry_map_.insert({path, SyncEntryInfo()});
  if (!p.second) {
    LOG(ERROR) << "insert into sync entry map failed, may have had this file already";
    return -1;
  }
  auto it = p.first;
  it->second.filename = &it->first;
  it->second.sent = offset;
  it->second.state = SyncEntryState::SYNCING;
  it->second.target = target;

  using namespace std::placeholders;
  auto* fs_file = new FSFile(get_loop(), path, std::bind(&SyncClient::file_cb, this, _1, _2));
  auto pr = fs_files_map_.insert({path,fs_file });
  if (!pr.second) {
    LOG(ERROR) << "insert into fs file ,ap failed " << path << " may have had this file already";
    //TODO erase this file from sync entry map
    return -1;
  }
  pr.first->second->stat();
  return 0;
}

int SyncClient::file_cb(uv_fs_t* fs, uv_fs_type fs_type)
{
  auto* file = (FSFile*)fs->data;
  auto it_info = sync_entry_map_.find(file->file_name());
  switch (fs_type) {
    case UV_FS_STAT:
      {
        LOG(DEBUG) << "on fs stat " << file->file_name();
        it_info->second.total_len = fs->statbuf.st_size;
        LOG(DEBUG) << "file: " << file->file_name() << " size: " << fs->statbuf.st_size;
        auto* file = (FSFile*)fs->data;
        file->open(UV_FS_O_RDONLY, 0);
        break;
      }
    case UV_FS_READ:
      {
        auto bytes_read = file->read_buf().total_len();
        LOG(DEBUG) << "on fs read " << file->file_name() << " read buf size: " << bytes_read;
        auto ret = send_deposite_file_message(file->file_name().c_str(),
                                   it_info->second.total_len,
                                   it_info->second.sent,
                                   file->read_buf());
        if ( ret < 0) {
          LOG(ERROR) << "sending file: " << it_info->second.filename << " error";
          LOG(ERROR) << "error at: [" << it_info->second.sent+1 << "] len: [" << uv_fs_get_result(fs) <<"]";
          file->close();
          break;
        } else if (ret == 0) {
          is_wrote_too_much_ = true;
          fses_.insert(fs);
          break;
        }
        it_info->second.sent += bytes_read;
        LOG(DEBUG) << "sent: " << it_info->second.sent << " target: " << it_info->second.target << " total: " << it_info->second.total_len;
        if (it_info->second.sent < it_info->second.total_len) {
        } else {
          file->close();
          break;
        }
      }
    case UV_FS_OPEN:
      {
        auto sent = it_info->second.sent;
        auto target = it_info->second.target;
        target = target == 0 ? it_info->second.total_len : target;
        auto to_read = sent + 4096 > target ? target - sent : 4096;
        LOG(DEBUG) << "on fs open " << file->file_name() << " start read: " << to_read << " start from: " << it_info->second.sent;
        if (target - sent > 0) {
          file->read(to_read, it_info->second.sent);
        }
        break;
      }
    case UV_FS_CLOSE:
      {
        LOG(DEBUG) << "on fs close " << file->file_name();
        sync_entry_map_.erase(it_info);
        auto it = fs_files_map_.find(file->file_name());
        delete it->second;
        fs_files_map_.erase(it);
        break;
      }
    default:
      break;
  }
  return 0;
}

int SyncClient::send_deposite_file_message(const char* file_name, uint64_t len, uint64_t from, reactor::buffer& data)
{
  auto tmp_to = from;
  uint32_t len_to_pullup = 4096;
  while(data.total_len() > 0) {
    if (data.total_len() < 4096) {
      len_to_pullup = data.total_len();
    }
    auto* d = data.pullup(len_to_pullup);
    tmp_to += len_to_pullup - 1;
    auto package = filesync::getDepositeFilePackage(fs_files_path_map_[file_name].c_str(), len, from, tmp_to, d);
    int64_t size = package->ByteSizeLong();
    package->SerializeToArray(mes_, size);
    LOG(DEBUG) << "build package size: " << size;
    auto ret = write(size, false, size + 64);
    //if we got error when writing, should we clear all data in this buffer
    //the only reason that this could happen will be the connection error
    if (ret <= 0 || (ret = write(mes_, size, true, 0)) < 0) {
      LOG(WARNING) << "writing file: " << file_name << " from: " << from << " to: " << tmp_to << " return 0 or -1";
      return ret;
    }
    data.drain(len_to_pullup);
    LOG(INFO) << "writing file: " << file_name << " from: " << from << " to: " << tmp_to << " succeed " << len_to_pullup;
  }
  assert(data.total_len() == 0);
  return 1;
}

}
