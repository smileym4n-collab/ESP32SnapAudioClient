#include "audio_dsp.h"
#include "channel_mode.h"
#include <cassert>
#include <cmath>
#include <complex>
#include <cstdio>
#include <limits>
#include <thread>

using namespace audio_dsp;
struct TestEngine : Engine {
  Workspace storage;
  void begin() { assert(Engine::begin(&storage)); }
};
using Complex = std::complex<double>;
Complex response(const Prepared &p, unsigned ch, double hz) {
  const Complex z = std::polar(1.0, -2*3.141592653589793*hz/p.rate);
  Complex h = 1;
  for (unsigned i = 0; i < p.filterCount[ch]; ++i) {
    const auto &b = p.filters[ch][i];
    h *= (double(b.b0)+double(b.b1)*z+double(b.b2)*z*z)/
         (1.0+double(b.a1)*z+double(b.a2)*z*z);
  }
  return h;
}
Prepared make(Config c = Config{}, unsigned rate = 48000, float tone = 0) {
  Prepared p;
  const char *error;
  assert(prepare(c, rate, tone, -30, p, error));
  return p;
}
void run(Engine &e, int16_t *in, int16_t *out, unsigned blocks = 30, unsigned rate = 48000) {
  for (unsigned i=0; i<blocks; ++i) e.process(in, out, 128, rate);
}
int main() {
  for (unsigned rate : {32000u, 44100u, 48000u, 96000u}) {
    for (float fc : {100.f, 2400.f, rate*0.45f}) {
      Config c; c.crossoverHz = fc;
      auto p = make(c, rate);
      auto high = response(p, 0, fc), low = response(p, 1, fc);
      assert(fabs(abs(high)-0.5) < 0.0005 && fabs(abs(low)-0.5) < 0.0005);
      assert(fabs(arg(high/low)) < 0.001);
      for (unsigned i = 1; i < 100; ++i) {
        const double f = rate*0.49*i/100;
        assert(fabs(abs(response(p,0,f)+response(p,1,f))-1) < 0.002);
      }
    }
  }
  auto p = make();
  assert(abs(response(p,0,100)) < 0.00001);
  assert(abs(response(p,1,10000)) < 0.002);
  printf("48 kHz LR4 Fc: HIGH %.6f dB, LOW %.6f dB, phase delta %.6f deg\n",
      20*log10(abs(response(p,0,2400))), 20*log10(abs(response(p,1,2400))),
      arg(response(p,0,2400)/response(p,1,2400))*180/3.141592653589793);
  const char *error;
  Config c;
  c.output[0].peq[0].enabled=1;
  c.output[0].peq[0].frequency=1000;
  c.output[0].peq[0].gainDb=6;
  auto peak=make(c);
  assert(fabs(abs(response(peak,0,1000)/response(p,0,1000))-dbGain(6)) < 0.001);
  c.output[0].peq[0].type=FilterType::Notch;
  assert(abs(response(make(c),0,1000)) < 0.00001);
  for (auto type : {FilterType::LowShelf, FilterType::HighShelf}) {
    c.output[0].peq[0].type=type;
    auto shelf=make(c);
    const double f= type == FilterType::LowShelf ? 1 : 23999;
    const auto &b=shelf.filters[0][2];
    const Complex z=std::polar(1.0,-2*3.141592653589793*f/48000);
    const double gain=abs((double(b.b0)+double(b.b1)*z+double(b.b2)*z*z)/(1.0+double(b.a1)*z+double(b.a2)*z*z));
    assert(fabs(gain-dbGain(6))<0.001);
  }
  c=Config{}; c.crossoverHz=24000; assert(!validate(c,48000,error));
  c=Config{}; c.output[0].peq[0].q=0; assert(!validate(c,48000,error));
  c=Config{}; c.output[1].delayMs=10.01; assert(!validate(c,48000,error));
  c=Config{}; c.masterDb=std::numeric_limits<float>::quiet_NaN(); assert(!validate(c,48000,error));
  c=Config{}; c.schema=99; assert(!validate(c,48000,error));
  c=Config{}; c.highRight=2; assert(!validate(c,48000,error));
  assert(!prepare(Config{},48000,1000,-6,p,error));

  int16_t in[256], out[256], swapped[256];
  Engine unavailable;
  assert(!unavailable.begin(nullptr));
  assert(!unavailable.publish(make()));
  unavailable.setAllowed(true);
  for (auto &x : out) x = 123;
  unavailable.process(in, out, 128, 48000);
  for (auto x : out) assert(x == 0);
  for (unsigned i=0;i<128;++i) { in[2*i]=12000; in[2*i+1]=-12000; }
  TestEngine e; e.begin(); e.publish(make());
  run(e,in,out); for (auto x:out) assert(x==0);  // startup held at zero
  e.setAllowed(true); run(e,in,out); for (auto x:out) assert(x==0);  // L+R cancellation
  app_config::routeStereo16(app_config::ChannelMode::Left,(uint8_t*)in,sizeof(in),(uint8_t*)in);
  run(e,in,out,100); assert(abs(out[255]-12000)<5 && abs(out[254])<5);
  Config right; right.highRight=1;
  TestEngine other; other.begin(); other.publish(make(right)); other.setAllowed(true);
  run(other,in,swapped,100); assert(abs(swapped[254]-out[255])<5);
  // R selection occurs upstream; selecting an all-zero source cannot create programme output.
  for(unsigned i=0;i<128;++i) {in[2*i]=12000;in[2*i+1]=0;}
  app_config::routeStereo16(app_config::ChannelMode::Right,(uint8_t*)in,sizeof(in),(uint8_t*)in);
  run(e,in,out,100); for (auto x:out) assert(abs(x)<5);
  run(e,in,out,1,44100); for (auto x:out) assert(x==0); // rate mismatch
  // Integer delay/polarity and output map are semantic, before transport routing.
  Config delayed; delayed.output[0].delayMs=1; delayed.output[1].inverted=1;
  auto dp=make(delayed); assert(dp.delays[0]==48 && dp.gains[1]==-1);
  TestEngine base, timeShift; base.begin(); timeShift.begin();
  base.publish(make()); timeShift.publish(dp); base.setAllowed(true);timeShift.setAllowed(true);
  run(base,in,out); run(timeShift,in,swapped);
  int16_t history[48]={};
  for(unsigned i=0;i<2000;++i) {
    int16_t impulse[2]={int16_t(i==0?10000:0),int16_t(i==0?10000:0)}, a[2],b[2];
    base.process(impulse,a,1,48000);timeShift.process(impulse,b,1,48000);
    assert(b[0]==history[i%48]);history[i%48]=a[0];assert(abs(int(b[1])+int(a[1]))<=1);
  }
  c=Config{}; c.output[1].gainDb=6;
  assert(e.publish(make(c))); assert(!e.publish(make()));
  for(auto &x:in)x=32767;
  run(e,in,out,100); assert(e.clipped()>0 && out[255]==32767);
  c.output[1].muted=1;assert(e.publish(make(c)));run(e,in,out,100);assert(out[255]==0);
  TestEngine tone; tone.begin();tone.publish(make(Config{},48000,2400));tone.setAllowed(true);
  for(auto &x:in)x=0;
  run(tone,in,out,100);int peakValue=0;for(auto x:out)peakValue=std::max(peakValue,abs(int(x)));
  assert(peakValue>490 && peakValue<525);
  assert(tone.publish(make()));run(tone,in,out,100);for(auto x:out)assert(x==0);
  // Stress the single-producer mailbox while the audio consumer runs.
  TestEngine concurrent; concurrent.begin(); concurrent.setAllowed(true);
  std::atomic<bool> done{false};
  std::thread producer([&]{for(unsigned i=0;i<2000;++i){Config v;v.highRight=i%2;
    while(!concurrent.publish(make(v))) std::this_thread::yield();}done.store(true);});
  while(!done.load())concurrent.process(in,out,128,48000);
  producer.join();
  printf("DSP core checks passed; Engine=%zu Config=%zu Prepared=%zu bytes\n",sizeof(Engine),sizeof(Config),sizeof(Prepared));
}
