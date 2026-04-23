#if !defined(HANDMADE_H)
#define HANDMADE_H


/* NOTE:
 *  HANDMADE_INTERNAL:
 *    0: Build for public release
 *    1: Build for developers only
 *  HANDMADE_SLOW:
 *    0: No slow code allowed
 *    1: slow code welcome
 */
#if HANDMADE_SLOW
#define Assert(expression) if(!(expression)) {*(volatile int*)0 = 0;}
#else
#define Assert(expression)
#endif
#define Kilobytes(value) (value * 1024)
#define Megabytes(value) (Kilobytes(value) * 1024)
#define Gigabytes(value) (Megabytes(value) * 1024)
#define Terabytes(value) (Gigabytes(value) * 1024)
#define ArrayCount(array) sizeof(array) / sizeof((array)[0])

inline uint32_t SafeTruncateUInt64(uint64_t value) {
  Assert(value <= 0xFFFFFFFF);
  uint32_t result = (uint32_t)value;
  return result;
}

/*
 * TODO: Services that the platform layer provides to the game
 */
#if HANDMADE_INTERNAL
struct DEBUGReadFileResult {
  uint32_t contents_size;
  void* contents;
};

internal DEBUGReadFileResult DEBUGPlatformReadEntireFile(char* file_name);
internal void DEBUGPlatformFreeFileMemory(void* memory);

internal bool DEBUGPlatformWriteEntireFile(char* file_name, uint32_t memory_size, void* memory);
#endif

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

struct GameButtonState {
  int half_transition_count;
  bool ended_down;
};

struct GameControllerInput {
  bool is_analog;

  float start_x;
  float start_y;

  float min_x;
  float min_y;

  float max_x;
  float max_y;

  float end_x;
  float end_y;

  union {
    GameButtonState buttons[6];
    struct {
      GameButtonState up;
      GameButtonState down;
      GameButtonState left;
      GameButtonState right;
      GameButtonState left_shoulder;
      GameButtonState right_shoulder;
    };
  };
};

struct GameInput {
  GameControllerInput controllers[4];
};

struct GameMemory {
  bool is_initialized;

  uint64_t permanent_storage_size;
  void * permanent_storage;

  uint64_t transient_storage_size;
  void * transient_storage;
};

internal void GameUpdateAndRender(GameMemory* memory, GameInput* input,
                                  GameOffscreenBuffer* buffer,
                                  GameSoundOutputBuffer* sound_buffer);


struct GameState {
  int tone_hz;
  int x_offset;
  int y_offset;
};

#endif
