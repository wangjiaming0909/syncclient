#include "sync_package.h"
#include "easylogging++.h"

namespace filesync
{

SyncPackagePtr getHelloPackage(const char* hello, PackageType clientOrServer)
{
  SyncPackagePtr ret = std::make_shared<SyncPackage>();
  auto* header = ret->mutable_header();
  header->set_type(clientOrServer);
  if (clientOrServer == PackageType::Client)
    header->set_command(Command::ClientHello);
  else if (clientOrServer == PackageType::Server)
    header->set_command(Command::ServerHello);
  header->set_headercontentlen(0);
  header->set_contenttype(HeaderContentType::None);
  ret->set_contentlen(strlen(hello));
  ret->set_content(hello);
  ret->set_len(ret->ByteSizeLong());
  return ret;
}

SyncPackagePtr getDepositeFilePackage(const char* fileName, uint64_t fileLen, uint64_t from, uint64_t to, void* data)
{
  LOG(DEBUG) << "Build deposite file package filename: " << fileName << " fileLen: " << fileLen << " from: " << from << " to: " << to;
  SyncPackagePtr p = std::make_shared<SyncPackage>();
  auto* header = p->mutable_header();
  header->set_type(PackageType::Client);
  header->set_command(Command::DepositeFile);
  header->set_contenttype(HeaderContentType::DepositeFileContent);
  header->set_headercontentlen(0);
  auto* depositeFileHeader = header->mutable_depositefileheader();
  depositeFileHeader->set_filename(fileName);
  depositeFileHeader->set_filelen(fileLen);
  depositeFileHeader->set_curseqstart(from);
  depositeFileHeader->set_curseqend(to);
  p->set_contentlen(to - from + 1);
  p->set_content(data, p->contentlen());
  p->set_len(p->ByteSizeLong());
  return p;
}

SyncPackagePtr getReportStatePackage(DepositeState state)
{
  SyncPackagePtr p = std::make_shared<SyncPackage>();
  auto* header = p->mutable_header();
  header->set_type(PackageType::Server);
  header->set_command(Command::ReportState);
  header->set_headercontentlen(0);
  header->set_contenttype(HeaderContentType::DepositeStateContent);
  header->set_depositestateheader(state);
  p->set_contentlen(0);
  p->set_content("No Content");
  p->set_len(p->ByteSizeLong());
  return p;
}

}
/*
namespace filesync
{
SyncPackagePtr getReportStatePackage(DepositeState state)
{
  SyncPackagePtr p = std::make_shared<SyncPackage>();
  auto& header = p->header;

  return p;
}

}

*/
