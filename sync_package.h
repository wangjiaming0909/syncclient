#pragma once
#include "sync_mess.pb.h"
#include <memory>

namespace filesync
{
  using SyncPackagePtr = std::shared_ptr<SyncPackage>;

  SyncPackagePtr getHelloPackage(const char* hello, PackageType clientOrServer);
  SyncPackagePtr getDepositeFilePackage(const char* fileName, uint64_t fileLen, uint64_t from, uint64_t to, void* data);
  SyncPackagePtr getReportStatePackage(DepositeState state);
}

/*
#include <sstream>
#include <stdint.h>
#include <memory>
#include <string>
#include <type_traits>
#include "reactor/buffer.h"


namespace filesync
{


enum class PackageType : uint8_t
{
  Client = 1, Server = 2
};

enum class Command : uint8_t
{
  ClientHello = 1,
  ServerHello = 2,
  DepositeFile = 3,
  ResportState = 4,
};

struct Header
{
  ~Header() {free(headerContent);}
  PackageType type;
  Command command;
  uint32_t headerContentLen;
  void* headerContent;
};


template <typename T, bool b>
struct HeaderRestriction_
{
  using value = std::false_type;
};

template <typename T>
struct HeaderRestriction_<T, true>
{
  using value = std::true_type;
};

template <typename T>
struct HeaderRestriction
{
  using restriction = typename HeaderRestriction_<T, std::is_trivial<T>::value>::value;
};


class A
{
};

using res = HeaderRestriction<A>::restriction;


struct DepositeFileHeader //: HeaderRestriction<DepositeFileHeader>
{
  uint32_t fileNameLen;
  //std::string fileName;
  const char* fileName;
  uint64_t fileLen;
  uint64_t curSeqStart;
  uint64_t curSeqEnd;
};

static_assert(HeaderRestriction<A>::restriction::value, "");
static_assert(HeaderRestriction<int>::restriction::value, "");
static_assert(HeaderRestriction<DepositeFileHeader>::restriction::value, "");

enum class DepositeState : uint8_t
{
  Succeed, Failed, Paused
};

struct SyncPackage
{
  ~SyncPackage() { if (data_need_free) free(content); }
  Header header;
  uint64_t contentLen;
  void* content;
  bool data_need_free = false;
};

using SyncPackagePtr = std::shared_ptr<SyncPackage>;

SyncPackagePtr getClientHelloPackage(const char* helloContent);
SyncPackagePtr getServerHelloPackage(const char* helloContent);
SyncPackagePtr getDepositeFilePackage(const char* fileName, uint64_t fileLen, uint64_t from, uint64_t to, void* data, bool data_can_persist);
SyncPackagePtr getReportStatePackage(DepositeState state);

}


*/
