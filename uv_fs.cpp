#include "uv_fs.h"
#include "easylogging++.h"

namespace uv
{

FSFile::FSFile(uv_loop_t* loop, const char* file_name, Callback cb)
 : loop_(loop), file_name_(file_name), cb_(cb)
{
  fs_ = new uv_fs_t();
  fs_->data = this;
}

FSFile::~FSFile()
{
  delete fs_;
}
int FSFile::open(int flags, int mode)
{
  auto h = uv_fs_open(loop_, fs_, file_name_.c_str(), flags, mode, fs_callback);
  LOG(DEBUG) << "open file: " << file_name_;
  if (h < 0) {
    LOG(ERROR) << "open file " << file_name_ << " failed";
  }
  return h;
}
int FSFile::close()
{
  auto ret = uv_fs_close(loop_, fs_, handle_, fs_callback);
  if (ret < 0) {
    LOG(ERROR) << "close file " << file_name_ << " failed";
  }
  return ret;
}
int FSFile::read(uint32_t size, int64_t offset)
{
  uv_read_buf_.base = (char*)::calloc(size, 1);
  uv_read_buf_.len = size;
  auto ret = uv_fs_read(loop_, fs_, handle_, &uv_read_buf_, 1, offset, fs_callback);
  if (ret < 0) {
    LOG(ERROR) << "read file " << file_name_ << " failed";
  }
  return ret;
}
int FSFile::write()
{
  return 0;
}

int FSFile::stat()
{
  auto ret = uv_fs_stat(loop_, fs_, file_name_.c_str(), fs_callback);
  if (ret < 0) {
    LOG(ERROR) << "fs stat file: " << file_name_ << " failed";
  }
  return ret;
}

void fs_callback(uv_fs_t* req)
{
  auto* f = (FSFile*)req->data;
  int res = (int)uv_fs_get_result(req);
  if (res < 0) {
    LOG(DEBUG) << "when doing: " << uv_fs_get_type(req) << " error: " << uv_strerror(res);
  }
  switch (req->fs_type)
  {
    case UV_FS_OPEN:
      f->handle_ = req->result;
      break;
    case UV_FS_CLOSE:
    case UV_FS_WRITE:
      break;
    case UV_FS_READ:
      {
        if (res < 0) {
          req->fs_type = UV_FS_CLOSE;
        } else {
          f->read_buf_.append(f->uv_read_buf_.base, f->uv_read_buf_.len);
        }
        free(f->uv_read_buf_.base);
        f->uv_read_buf_.base = nullptr;
        f->uv_read_buf_.len = 0;
        break;
      }

    case UV_FS_STAT:
      break;
    default:
      break;
  }
  f->cb_(req, req->fs_type);
}

}
