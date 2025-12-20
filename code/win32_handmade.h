#if !defined(WIN32_HANDMADE_H)
#define WIN32_HANDMADE_H

struct Win32OffscreenBuffer {
  BITMAPINFO info;
  void* memory;
  int width;
  int height;
  int pitch;
  int bytes_per_pixel;
};

struct Win32WindowDimension {
  int width;
  int height;
};

struct Win32SoundOutput {
  int samples_per_second;
  int bytes_per_sample;
  int secondary_buffer_size;
  uint32_t sample_index;
  int wave_period;
  float t_sine;
  int latency_sample_count;
};

#endif
