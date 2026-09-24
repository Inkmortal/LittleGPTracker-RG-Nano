#include "Logger.h"
#include <iostream>
#include <stdio.h>
#include <string>

void StdOutLogger::AddLine(const char *line)
{
	std::cout << line << std::endl ;
}

// ----------------------------------------------

FileLogger::FileLogger(const Path &path)
:path_(path)
,file_(0)
,size_(0)
{
}

FileLogger::~FileLogger()
{
  if (file_)
  {
    fclose(file_);
  }
}

// The log of the last run is kept as <name>.prev (a crash is usually found
// after restarting); the file stays open and every line is flushed, so the
// last lines before a crash are on the card
Result FileLogger::Init()
{
	std::string path=path_.GetPath() ;
	std::string prev=path+".prev" ;
	remove(prev.c_str()) ;
	rename(path.c_str(),prev.c_str()) ;
	file_= fopen(path.c_str(),"w") ;
  if (!file_)
  {
    return Result("Failed to open log file");
  }
  size_=0 ;
  return Result::NoError;
}

void FileLogger::AddLine(const char *line)
{
	// Called from the UI and the audio thread
	SysMutexLocker lock(mutex_) ;
	if (!file_) return ;
	if (size_>FILE_LOGGER_MAX_BYTES) {
		// Keep the card from filling up in a long session: start over,
		// keeping the previous half in <name>.old
		fclose(file_) ;
		std::string path=path_.GetPath() ;
		std::string old=path+".old" ;
		remove(old.c_str()) ;
		rename(path.c_str(),old.c_str()) ;
		file_=fopen(path.c_str(),"w") ;
		size_=0 ;
		if (!file_) return ;
	}
	int n=fprintf(file_,"%s\n",line) ;
	if (n>0) size_+=n ;
	fflush(file_) ;
}
