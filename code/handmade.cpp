#include "handmade.h"

internal void RenderWeirdGradient(GameOffscreenBuffer* buffer,
                                  int x_offset,
                                  int y_offset) {
  uint8_t* row = (uint8_t*)buffer->memory;
  for (int y = 0; y < buffer->height; ++y) {
    uint32_t* pixel = (uint32_t*)row;
    for (int x = 0; x < buffer->width; ++x) {
      // LITTLE ENDIAN FORMAT
      // pixel in memory: BB GG RR xx
      // pixel in register: xx RR GG BB
      uint8_t blue = (x + x_offset);
      uint8_t green = (y + y_offset);

      *pixel++ = ((green << 8) | blue);
    }

    row += buffer->pitch;
  }
}

internal void GameUpdateAndRender(GameOffscreenBuffer* buffer,
                                  int x_offset,
                                  int y_offset) {
  RenderWeirdGradient(buffer, x_offset, y_offset);
}
