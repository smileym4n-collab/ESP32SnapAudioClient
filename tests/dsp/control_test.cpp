#include <cassert>
#include "../../src/dsp_control.cpp"
using namespace dsp_control;
void cmd(const char *s) {char b[192];strcpy(b,s);command(b);}
int main() {
  begin(false);
  assert(dacConfigured && dacOkay);
  assert(config.crossoverHz==2400 && toneHz==0 && flashWrites==0);
  cmd("dsp crossover 1800");cmd("dsp map high-right");cmd("dsp high gain -3.5");
  cmd("dsp low delay 1.25");cmd("dsp high polarity invert");cmd("dsp low mute on");
  cmd("dsp high peq 2 lowshelf 900 0.707 -4");cmd("dsp high peq 2 off");
  cmd("dsp low peq 6 notch 350 3");cmd("dsp tone 1000");
  assert(config.highRight && config.output[0].gainDb==-3.5 && toneHz==1000 && toneDb==-30);
  assert(flashWrites==0);cmd("dsp save");assert(flashWrites==1);
  auto expected=config;
  cmd("dsp defaults");assert(config.crossoverHz==2400 && toneHz==0 && flashWrites==1);
  // Simulate fresh boot settings restore; mailbox itself is exercised in core test.
  begin(false);
  assert(memcmp(&expected,&config,sizeof(config))==0 && toneHz==0);
  const auto before=config;
  for(const char *bad : {"dsp crossover nan","dsp high peq 1 peak 1000 0 6",
      "dsp high peq 1.5 clear","dsp low delay -1","dsp master 1","dsp high gain 7",
      "dsp tone 1000 0","dsp tone -1","dsp tone 0","dsp map left","dsp high peq 7 clear"}) cmd(bad);
  assert(memcmp(&before,&config,sizeof(config))==0 && toneHz==0);
  cmd("dsp high peq 2 on");assert(config.output[0].peq[1].enabled);
  cmd("dsp high peq clear");for(auto &p:config.output[0].peq)assert(!p.enabled);
  cmd("dsp low peq 6 clear");assert(!config.output[1].peq[5].enabled);
  cmd("dsp status");assert(Serial.output.find("HIGH=RIGHT")!=std::string::npos);
  cmd("dsp dac");
  assert(Serial.output.find("[dsp-dac] readback=OK")!=std::string::npos);
  assert(Serial.output.find("ref=0x10 dacSrc=0x10 P=1 J=32 D=0 R=2 DDSP=2 DDAC=16 DNCP=4 DOSR=8 IDAC=1024 flex=0x1A")!=std::string::npos);
  // Playback has not started in this host test, so both soft-mute bits remain set.
  assert(Serial.output.find("mute=0x11 format=0x00 volumeL=0x30 volumeR=0x30")!=std::string::npos);
  savedConfig[0]=99;config=Config{};begin(false);assert(config.crossoverHz==2400);
  savedConfig.resize(3);config=Config{};begin(false);assert(config.crossoverHz==2400);
  int16_t silence[256]={}, output[256];
  outputStarted(true);
  for(unsigned i=0;i<40;++i){process(silence,output,128);service();}
  cmd("dsp crossover 21000");cmd("dsp tone 1000");
  for(unsigned i=0;i<40;++i){service();process(silence,output,128);}
  format(44100,2,16);service();assert(!preparedValid);
  for(unsigned i=0;i<3;++i){service();process(silence,output,128);for(auto x:output)assert(x==0);}
  cmd("dsp tone off");assert(toneHz==0);
  cmd("dsp defaults");
  for(unsigned i=0;i<40;++i){service();process(silence,output,128);}
  assert(preparedValid && engine.appliedRate()==44100);
  Wire1.ack = 2;  // Simulate missing DAC/address NACK.
  hardwareBegin();
  assert(!dacOkay);
  service();
  for (auto &x : silence) x = 12000;
  process(silence,output,128);
  for (auto x : output) assert(x == 0);
  Wire1.ack = 0;
  puts("DSP shell and mocked NVS restore/defaults/tone-exclusion checks passed");
}
