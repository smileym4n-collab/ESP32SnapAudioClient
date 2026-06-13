#pragma once

#include <string.h>

#include <Arduino.h>

namespace app_config {

enum class ChannelMode : uint8_t { Stereo, Left, Right };

inline const char *channelModeName(ChannelMode mode) {
  switch (mode) {
    case ChannelMode::Stereo:
      return "stereo";
    case ChannelMode::Left:
      return "left";
    case ChannelMode::Right:
      return "right";
    default:
      return "stereo";
  }
}

inline bool parseChannelMode(const String &value, ChannelMode &mode) {
  if (value == "stereo") {
    mode = ChannelMode::Stereo;
    return true;
  }
  if (value == "left") {
    mode = ChannelMode::Left;
    return true;
  }
  if (value == "right") {
    mode = ChannelMode::Right;
    return true;
  }
  return false;
}

// Routes interleaved 16-bit stereo PCM according to the channel mode, writing
// the result to dst (which must be at least len bytes). For Left/Right the
// selected input channel is duplicated to both output channels; Stereo is a
// straight copy. Output length always equals input length, and any trailing
// partial frame is copied through unchanged. Shared by every PCM output path
// that needs channel routing so behavior stays identical.
inline void routeStereo16(ChannelMode mode, const uint8_t *src, size_t len,
                          uint8_t *dst) {
  constexpr size_t kBytesPerFrame = sizeof(int16_t) * 2;

  if (mode == ChannelMode::Stereo) {
    memcpy(dst, src, len);
    return;
  }

  const int16_t *in = reinterpret_cast<const int16_t *>(src);
  int16_t *out = reinterpret_cast<int16_t *>(dst);
  const size_t frameCount = len / kBytesPerFrame;

  for (size_t i = 0; i < frameCount; ++i) {
    const int16_t selected =
        mode == ChannelMode::Left ? in[i * 2] : in[(i * 2) + 1];
    out[i * 2] = selected;
    out[(i * 2) + 1] = selected;
  }

  const size_t routedBytes = frameCount * kBytesPerFrame;
  if (routedBytes < len) {
    memcpy(dst + routedBytes, src + routedBytes, len - routedBytes);
  }
}

}  // namespace app_config
