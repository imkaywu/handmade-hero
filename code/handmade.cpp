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
      uint8_t blue = (uint8_t)(x + x_offset);
      uint8_t green = (uint8_t)(y + y_offset);

      *pixel++ = ((green << 8) | blue);
    }

    row += buffer->pitch;
  }
}

void GameOutputSound(GameSoundOutputBuffer* sound_buffer, int tone_hz) {
  local_persist float t_sine;

  int16_t tone_volume = 3000;
  int wave_period = sound_buffer->samples_per_second / tone_hz;

  int16_t* sample_out = sound_buffer->samples;
  for (int i = 0; i < sound_buffer->sample_count; ++i) {
    float sine_value = sinf(float(2.0 * PI * t_sine));
    int16_t sample_value = (int16_t)(sine_value * tone_volume);
    *sample_out++ = sample_value;
    *sample_out++ = sample_value;

    t_sine += 1.0f / wave_period;
  }
}

internal void GameUpdateAndRender(GameMemory* memory,
                                  GameInput* input,
                                  GameOffscreenBuffer* buffer,
                                  GameSoundOutputBuffer* sound_buffer) {
  Assert(sizeof(GameState) <= memory->permanent_storage_size);

  GameState* game_state = (GameState*)memory->permanent_storage;
  if (!memory->is_initialized) {
    char* file_name = __FILE__;
    DEBUGReadFileResult file = DEBUGPlatformReadEntireFile(file_name);
    if (file.contents) {
      DEBUGPlatformWriteEntireFile(
          "test.out", file.contents_size, file.contents);
      DEBUGPlatformFreeFileMemory(file.contents);
    }

    game_state->tone_hz = 256;

    // TODO: this might be more appropriate to do in platform layer
    memory->is_initialized = true;
  }

  GameControllerInput* input0 = &input->controllers[0];
  if (input0->is_analog) {
    // NOTE: using analog movment tuning
    game_state->x_offset += (int)(4 * input0->end_x);
    game_state->tone_hz = 256 + (int)(128 * input0->end_y);
  } else {
    // NOTE: using digital movment tuning
  }

  if (input0->down.ended_down) {
    game_state->y_offset += 1;
  }

  GameOutputSound(sound_buffer, game_state->tone_hz);
  RenderWeirdGradient(buffer, game_state->x_offset, game_state->y_offset);
}
