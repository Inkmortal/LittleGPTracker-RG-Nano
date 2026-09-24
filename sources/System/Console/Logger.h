#pragma once

#include "System/Process/SysMutex.h"
#include "Trace.h"
#include "System/Errors/Result.h"
#include "System/FileSystem/FileSystem.h"

#include <stdio.h>

class StdOutLogger: public Trace::Logger
{
  virtual void AddLine(const char*);
};

#define FILE_LOGGER_MAX_BYTES (1024*1024)

class FileLogger: public Trace::Logger
{
public:
  FileLogger(const Path& path);
  ~FileLogger();

  Result Init();

private:
  virtual void AddLine(const char*);
  Path path_;
  FILE *file_;
  long size_;
  SysMutex mutex_;
};