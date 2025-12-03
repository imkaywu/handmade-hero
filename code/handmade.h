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

internal void GameUpdateAndRender(GameOffscreenBuffer* buffer);

#endif
