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

void GameOutputSound(GameSoundOutputBuffer* sound_buffer, int tone_hz) {
  local_persist float t_sine;

  int16_t tone_volume = 3000;
  int wave_period = sound_buffer->samples_per_second / tone_hz;

  int16_t* sample_out = sound_buffer->samples;
  for (int i = 0; i < sound_buffer->sample_count; ++i) {
    float sine_value = sinf(2.0 * PI * t_sine);
    int16_t sample_value = (int16_t)(sine_value * tone_volume);
    *sample_out++ = sample_value;
    *sample_out++ = sample_value;

    t_sine += 1.0f / wave_period;
  }
}

internal void GameUpdateAndRender(GameInput* input,
                                  GameOffscreenBuffer* buffer,
                                  GameSoundOutputBuffer* sound_buffer) {
  local_persist int x_offset;
  local_persist int y_offset;
  local_persist int tone_hz = 256;

  GameControllerInput* input0 = &input->controllers[0];
  if (input0->is_analog) {
    // NOTE: using analog movment tuning
    x_offset += (int)(4 * input0->end_x);
    tone_hz = 256 + (int)(128 * input0->end_y);
  } else {
    // NOTE: using digital movment tuning
  }

  if (input0->down.ended_down) {
    y_offset += 1;
  }

  GameOutputSound(sound_buffer, tone_hz);
  RenderWeirdGradient(buffer, x_offset, y_offset);
}
