#pragma once
#include <vector>
#include <cstring>
static std::vector<uint8_t> savedConfig;
static unsigned flashWrites=0;
class Preferences {
public:
  bool begin(const char*,bool){return true;}
  void end(){}
  size_t getBytesLength(const char*){return savedConfig.size();}
  size_t getBytes(const char*,void* p,size_t n){if(n!=savedConfig.size())return 0;memcpy(p,savedConfig.data(),n);return n;}
  size_t putBytes(const char*,const void* p,size_t n){savedConfig.assign((const uint8_t*)p,(const uint8_t*)p+n);++flashWrites;return n;}
};
