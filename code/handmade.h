#if !defined(HANDMADE_H)
#define HANDMADE_H

/*
 * TODO: Services that the platform layer provides to the game
 */

/*
 * NOTE: Services that the game provides to the platform layer
 */

struct GameOffscreenBuffer {
  BITMAPINFO info;
  void* memory;
  int width;
  int height;
  int pitch;
  int bytes_per_pixel;
};

struct GameSoundOutputBuffer {
  int sample_count;
  int samples_per_second;
  int16_t *samples;
};

internal void GameUpdateAndRender(GameOffscreenBuffer* buffer, GameSoundOutputBuffer* sound_buffer, int tone_hz);

#endif
