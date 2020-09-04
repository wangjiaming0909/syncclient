#include "sync_client.h"
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

using namespace std;

void setupLogger()
{
  using namespace el;
  el::Loggers::addFlag(LoggingFlag::LogDetailedCrashReason);
  el::Loggers::addFlag(LoggingFlag::ColoredTerminalOutput);
  if (!el::Loggers::hasFlag(LoggingFlag::LogDetailedCrashReason)) exit(-1);
  if (!el::Loggers::hasFlag(LoggingFlag::ColoredTerminalOutput)) exit(-1);
  Configurations defaultConf;
  defaultConf.setToDefault();
  defaultConf.set(Level::Global, ConfigurationType::Enabled, "true");
  defaultConf.set(Level::Global, ConfigurationType::Format, "%datetime, %thread, %file, %level, %line, %msg");
  defaultConf.set(Level::Global, ConfigurationType::PerformanceTracking, "false");
  defaultConf.set(Level::Global, ConfigurationType::ToFile, "false");
  defaultConf.set(Level::Global, ConfigurationType::ToStandardOutput, "true");
  defaultConf.set(Level::Global, ConfigurationType::Filename, "./log");
  defaultConf.set(Level::Global, ConfigurationType::LogFlushThreshold, "100");
  defaultConf.set(Level::Global, ConfigurationType::MaxLogFileSize, "2097152");

  Loggers::reconfigureAllLoggers(defaultConf);
}

int main()
{
  setupLogger();
  sync_client::SyncClient client;
  //client.init("192.168.1.5", 9090);
  client.init("172.16.70.39", 9090);
  client.start_fs_monitoring("/home/wjm/tmp");
  return client.start();
}
