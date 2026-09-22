#pragma once
#include "audio_dsp.h"

namespace dsp_control {
void begin(bool bluetooth);
void service();
void command(char *line);
void format(unsigned rate, unsigned channels, unsigned bits);
void outputStarted(bool started);
void routing(unsigned mode);
void process(const int16_t *input, int16_t *output, size_t frames);
void stop();
}  // namespace dsp_control
