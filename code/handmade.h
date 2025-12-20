#if !defined(HANDMADE_H)
#define HANDMADE_H

#define ArrayCount(array) sizeof(array) / sizeof((array)[0])
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

internal void GameUpdateAndRender(GameInput* input,
                                  GameOffscreenBuffer* buffer,
                                  GameSoundOutputBuffer* sound_buffer);

#endif
