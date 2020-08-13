#include <iostream>
#include <uv.h>
#include "easylogging++.h"

INITIALIZE_EASYLOGGINGPP

using namespace std;

void setupLogger()
{
  using namespace el;
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
  cout << "Hello World!" << endl;
  uv_loop_t *loop = uv_default_loop();
  cout << loop->active_handles;
  return 0;
}
