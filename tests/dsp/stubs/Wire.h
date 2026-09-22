#pragma once
struct WireMock {
 int ack = 0;
 unsigned char registers[256] = {};
 unsigned char bytes[2] = {};
 unsigned char byteCount = 0;
 unsigned char readRegister = 0;
 bool begin(int,int,int){return true;}
 void setTimeOut(int){}
 void beginTransmission(int){byteCount=0;}
 void write(int value){if(byteCount<2)bytes[byteCount++]=value;}
 int endTransmission(bool=true){
   if(ack)return ack;
   if(byteCount)readRegister=bytes[0];
   if(byteCount==2)registers[bytes[0]]=bytes[1];
   return 0;
 }
 int requestFrom(int,int count){return ack?0:count;}
 int read(){return registers[readRegister];}
};
static WireMock Wire1;
