#pragma once
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <string>
#define LOW 0
#define HIGH 1
#define ADC_11db 3
using String = std::string;
struct SerialMock {
  std::string output;
  void println(const char *s) {output += s;output+='\n';}
  template<class... Args> void printf(const char *fmt, Args... args) {
    char b[2048]; snprintf(b,sizeof(b),fmt,args...);output+=b;
  }
};
static SerialMock Serial;
static uint32_t fakeMs=0;
inline uint32_t millis(){return fakeMs;}
inline uint32_t micros(){return fakeMs*1000;}
inline void analogReadResolution(int){}
inline void analogSetPinAttenuation(int,int){}
inline int analogRead(int){return 2048;}
