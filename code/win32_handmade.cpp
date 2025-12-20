#include <dsound.h>
#include <math.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#include <iomanip>
#include <iostream>

#define internal static
#define local_persist static
#define global_variable static

#define PI 3.1415926

#include "handmade.cpp"
#include "win32_handmade.h"

// TODO: Global variable for now.
global_variable bool running;
global_variable Win32OffscreenBuffer global_buffer;
global_variable LPDIRECTSOUNDBUFFER secondary_buffer;

// NOTE:
//   - MSVC x86/x64: __rdtsc()
//   - GCC/LLVM ARM64: __builtin_readcyclecounter()
//   - GCC/LLVM inline assembly
internal inline uint64_t rdtsc() {
  return __builtin_readcyclecounter();

  uint64_t cnt;
  __asm__ __volatile__("mrs %0, cntvct_el0" : "=r"(cnt));
  return cnt;
}

// NOTE: XInputGetState
#define X_INPUT_GET_STATE(name) \
  DWORD WINAPI name(DWORD user_index, XINPUT_STATE* state)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_get_state* XInputGetState_ = XInputGetStateStub;
#define XInputGetState XInputGetState_

// NOTE: XInputSetState
#define X_INPUT_SET_STATE(name) \
  DWORD WINAPI name(DWORD user_index, XINPUT_VIBRATION* vibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub) { return ERROR_DEVICE_NOT_CONNECTED; }
global_variable x_input_set_state* XInputSetState_ = XInputSetStateStub;
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) \
  HRESULT WINAPI name(LPCGUID guid_device, LPDIRECTSOUND* ds, LPUNKNOWN unknown)
typedef DIRECT_SOUND_CREATE(direct_sound_create);

internal void LoadXInput() {
  HMODULE xinput_library = LoadLibraryA("xinput1_4.dll");

  if (!xinput_library) {
    xinput_library = LoadLibraryA("xinput9_1_0.dll");
  }

  if (!xinput_library) {
    // TODO: Diagnostic
    xinput_library = LoadLibraryA("xinput1_3.dll");
  }

  if (xinput_library) {
    XInputGetState =
        (x_input_get_state*)GetProcAddress(xinput_library, "XInputGetState");
    XInputSetState =
        (x_input_set_state*)GetProcAddress(xinput_library, "XInputSetState");
  } else {
    // TODO: Diagnostic
  }
}

internal void Win32InitDSound(HWND window,
                              int32_t samples_per_second,
                              int32_t buffer_size) {
  // Load the library
  HMODULE dsound_library = LoadLibraryA("dsound.dll");
  if (dsound_library) {
    // NOTE: Get a DirectSound object - cooperative
    direct_sound_create* DirectSoundCreate =
        (direct_sound_create*)GetProcAddress(dsound_library,
                                             "DirectSoundCreate");
    LPDIRECTSOUND direct_sound;
    if (DirectSoundCreate &&
        SUCCEEDED(DirectSoundCreate(0, &direct_sound, 0))) {
      WAVEFORMATEX wave_format = {};
      wave_format.wFormatTag = WAVE_FORMAT_PCM;
      wave_format.nChannels = 2;
      wave_format.wBitsPerSample = 16;
      wave_format.nSamplesPerSec = samples_per_second;
      wave_format.nBlockAlign =
          wave_format.nChannels * wave_format.wBitsPerSample / 8;
      wave_format.nAvgBytesPerSec =
          wave_format.nSamplesPerSec * wave_format.nBlockAlign;
      wave_format.cbSize = 0;

      if (SUCCEEDED(
              direct_sound->SetCooperativeLevel(window, DSSCL_PRIORITY))) {
        DSBUFFERDESC buffer_description = {};
        buffer_description.dwSize = sizeof(DSBUFFERDESC);
        buffer_description.dwFlags = DSBCAPS_PRIMARYBUFFER;

        // Create a primary buffer
        LPDIRECTSOUNDBUFFER primary_buffer;
        if (SUCCEEDED(direct_sound->CreateSoundBuffer(
                &buffer_description, &primary_buffer, 0))) {
          if (SUCCEEDED(primary_buffer->SetFormat(&wave_format))) {
            std::cout << "Primary buffer format is set\n";
          } else {
            // TODO: Diagnostic
          }
        } else {
          // TODO: Diagnostic
        }
      } else {
        // TODO: Diagnostic
      }
      // Create a secondary buffer
      DSBUFFERDESC buffer_description = {};
      buffer_description.dwSize = sizeof(DSBUFFERDESC);
      buffer_description.dwFlags = 0;
      buffer_description.dwBufferBytes = buffer_size;
      buffer_description.lpwfxFormat = &wave_format;

      if (SUCCEEDED(direct_sound->CreateSoundBuffer(
              &buffer_description, &secondary_buffer, 0))) {
        // Start it playing
        std::cout << "Second buffer is created\n";
      }
    } else {
      // TODO: Diagnostic
    }
  } else {
    // TODO: Diagnostic
  }
}

internal void Win32ClearSoundBuffer(Win32SoundOutput* sound_output) {
  void* region1;
  DWORD region1_size;  // in bytes
  void* region2;
  DWORD region2_size;  // in bytes
  if (SUCCEEDED(secondary_buffer->Lock(0,
                                       sound_output->secondary_buffer_size,
                                       &region1,
                                       &region1_size,
                                       &region2,
                                       &region2_size,
                                       0))) {
    uint8_t* sample = (uint8_t*)region1;
    for (DWORD i = 0; i < region1_size; ++i) {
      *sample++ = 0;
    }

    sample = (uint8_t*)region2;
    for (DWORD i = 0; i < region2_size; ++i) {
      *sample++ = 0;
    }

    secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
  }
}

internal void Win32FillSoundBuffer(Win32SoundOutput* sound_output,
                                   DWORD byte_to_lock,
                                   DWORD bytes_to_write,
                                   GameSoundOutputBuffer* sound_buffer) {
  void* region1;
  DWORD region1_size;  // in bytes
  void* region2;
  DWORD region2_size;  // in bytes
  if (SUCCEEDED(secondary_buffer->Lock(byte_to_lock,
                                       bytes_to_write,
                                       &region1,
                                       &region1_size,
                                       &region2,
                                       &region2_size,
                                       0))) {
    // TODO: assert that region1_size and region2_size are valid
    int16_t* dst_sample = (int16_t*)region1;
    int16_t* src_sample = sound_buffer->samples;
    DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
    for (DWORD i = 0; i < region1_sample_count; ++i) {
      *dst_sample++ = *src_sample++;
      *dst_sample++ = *src_sample++;
      ++sound_output->sample_index;
    }

    dst_sample = (int16_t*)region2;
    DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
    for (DWORD i = 0; i < region2_sample_count; ++i) {
      *dst_sample++ = *src_sample++;
      *dst_sample++ = *src_sample++;
      ++sound_output->sample_index;
    }

    secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
  }
}

internal void Win32ProcessXInputDigitalButton(DWORD xinput_button_state,
                                              GameButtonState* old_state,
                                              DWORD button_bit,
                                              GameButtonState* new_state) {
  new_state->ended_down = ((xinput_button_state & button_bit) == button_bit);
  new_state->half_transition_count +=
      (old_state->ended_down != new_state->ended_down) ? 1 : 0;
}

internal Win32WindowDimension Win32GetWindowDimension(HWND window) {
  Win32WindowDimension result;

  RECT client_rect;
  GetClientRect(window, &client_rect);
  result.width = client_rect.right - client_rect.left;
  result.height = client_rect.bottom - client_rect.top;

  return result;
}

internal void Win32ResizeDIBSection(Win32OffscreenBuffer* buffer,
                                    int width,
                                    int height) {
  // TODO: Bulletbroof freeing DIBSection.
  // TODO: Maybe not free first, free after, then free first if that fails
  if (buffer->memory) {
    VirtualFree(buffer->memory, 0, MEM_RELEASE);
  }

  buffer->width = width;
  buffer->height = height;
  buffer->bytes_per_pixel = 4;
  buffer->pitch = buffer->width * buffer->bytes_per_pixel;

  buffer->info.bmiHeader.biSize = sizeof(buffer->info.bmiHeader);
  buffer->info.bmiHeader.biWidth = buffer->width;
  buffer->info.bmiHeader.biHeight = -buffer->height;
  buffer->info.bmiHeader.biPlanes = 1;
  buffer->info.bmiHeader.biBitCount = 32;
  buffer->info.bmiHeader.biCompression = BI_RGB;

  int bitmap_memory_size =
      buffer->width * buffer->height * buffer->bytes_per_pixel;
  buffer->memory = VirtualAlloc(
      0, bitmap_memory_size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
}

internal void Win32DisplayBufferInWindow(Win32OffscreenBuffer* buffer,
                                         HDC device_context,
                                         int window_width,
                                         int window_height) {
  StretchDIBits(device_context,
                0,
                0,
                window_width,
                window_height,
                0,
                0,
                buffer->width,
                buffer->height,
                buffer->memory,
                &buffer->info,
                DIB_RGB_COLORS,
                SRCCOPY);
}

LRESULT CALLBACK MainWindowCallback(HWND window,
                                    UINT message,
                                    WPARAM wparam,
                                    LPARAM lparam) {
  LRESULT result = 0;

  switch (message) {
    case WM_SIZE: {
      OutputDebugStringA("WM_SIZE");
    } break;
    case WM_CLOSE: {
      // TODO: Handle this with a message to user.
      running = false;
    } break;
    case WM_ACTIVATEAPP: {
      OutputDebugStringA("WM_ACTIVATEAPP");
    } break;
    case WM_DESTROY: {
      // TODO: Handle this as an error - recreate window?
      running = false;
    } break;
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      uint32_t vk_code = wparam;
      bool was_down = (lparam & (1 << 30)) != 0;
      bool is_down = (lparam & (1 << 31)) == 0;
      if (is_down != was_down) {
        if (vk_code == 'W') {
        } else if (vk_code == 'A') {
        } else if (vk_code == 'S') {
        } else if (vk_code == 'D') {
        } else if (vk_code == 'Q') {
        } else if (vk_code == 'E') {
        } else if (vk_code == VK_UP) {
        } else if (vk_code == VK_DOWN) {
        } else if (vk_code == VK_LEFT) {
        } else if (vk_code == VK_RIGHT) {
        } else if (vk_code == VK_SPACE) {
        } else if (vk_code == VK_ESCAPE) {
          std::cout << "ESCAPE: ";
          if (is_down) {
            std::cout << "is down ";
          }
          if (was_down) {
            std::cout << "was down\n";
          }
        }
      }

      bool alt_key_down = (lparam & (1 << 29)) != 0;
      if (vk_code == VK_F4 && alt_key_down) {
        running = false;
      }
    } break;
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC device_context = BeginPaint(window, &paint);

      Win32WindowDimension dimension = Win32GetWindowDimension(window);
      Win32DisplayBufferInWindow(
          &global_buffer, device_context, dimension.width, dimension.height);

      EndPaint(window, &paint);
    } break;
    default: {
      // OutputDebugStringA("default");
      result = DefWindowProc(window, message, wparam, lparam);
    } break;
  }

  return result;
}

int CALLBACK WinMain(HINSTANCE instance,
                     HINSTANCE prev_instance,
                     LPSTR cmd_line,
                     int cmd_show) {
  LARGE_INTEGER perf_count_frequency_result;
  QueryPerformanceFrequency(&perf_count_frequency_result);
  int64_t perf_count_frequency = perf_count_frequency_result.QuadPart;

  LoadXInput();

  Win32ResizeDIBSection(&global_buffer, 1280, 720);

  WNDCLASSA window_class = {};
  window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = MainWindowCallback;
  window_class.hInstance = instance;
  window_class.lpszClassName = "HandmadeHeroWindowClass";

  if (RegisterClassA(&window_class)) {
    HWND window = CreateWindowExA(0,
                                  window_class.lpszClassName,
                                  "Handmade Hero",
                                  WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  0,
                                  0,
                                  instance,
                                  0);

    if (window) {
      // NOTE: since we've specified CS_OWNDC, we can just get one
      // device_context and use it forever, since we are not sharing it.
      HDC device_context = GetDC(window);

      MSG message;

      Win32SoundOutput sound_output = {};
      sound_output.samples_per_second = 48000;
      sound_output.bytes_per_sample = sizeof(int16_t) * 2;
      sound_output.secondary_buffer_size =
          sound_output.samples_per_second * sound_output.bytes_per_sample;
      sound_output.latency_sample_count = sound_output.samples_per_second / 15;

      Win32InitDSound(window,
                      sound_output.samples_per_second,
                      sound_output.secondary_buffer_size);
      Win32ClearSoundBuffer(&sound_output);
      secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

      int16_t* samples =
          (int16_t*)VirtualAlloc(0,
                                 sound_output.secondary_buffer_size,
                                 MEM_RESERVE | MEM_COMMIT,
                                 PAGE_READWRITE);
#if HANDMADE_INTERNAL
      LPVOID base_address = (LPVOID)Gigabytes((uint64_t)2);
#else
      LPVOID base_address = 0;
#endif

      GameMemory game_memory = {};
      game_memory.permanent_storage_size = Megabytes(64);
      game_memory.transient_storage_size = Gigabytes(uint64_t(4));
      uint64_t total_storage_size = game_memory.permanent_storage_size +
                                    game_memory.transient_storage_size;
      game_memory.permanent_storage = VirtualAlloc(base_address,
                                                   total_storage_size,
                                                   MEM_RESERVE | MEM_COMMIT,
                                                   PAGE_READWRITE);
      game_memory.transient_storage = ((uint8_t*)game_memory.permanent_storage +
                                       game_memory.permanent_storage_size);

      if (samples && game_memory.permanent_storage &&
          game_memory.transient_storage) {
        GameInput inputs[2] = {};
        GameInput* old_input = &inputs[0];
        GameInput* new_input = &inputs[1];

        LARGE_INTEGER last_counter;
        QueryPerformanceCounter(&last_counter);

        uint64_t last_cycle_count = rdtsc();

        running = true;
        while (running) {
          while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
            if (message.message == WM_QUIT) {
              running = false;
            }
            TranslateMessage(&message);
            DispatchMessage(&message);
          }

          int max_controller_count = XUSER_MAX_COUNT;
          if (ArrayCount(new_input->controllers) < max_controller_count) {
            max_controller_count = ArrayCount(new_input->controllers);
          }
          for (DWORD i = 0; i < max_controller_count; ++i) {
            GameControllerInput old_controller = old_input->controllers[i];
            GameControllerInput new_controller = new_input->controllers[i];

            XINPUT_STATE input_state;
            if (XInputGetState(i, &input_state) == ERROR_SUCCESS) {
              // TODO: see if |input_state.dwPacketNumber| increments too
              // rapidly.
              XINPUT_GAMEPAD* pad = &input_state.Gamepad;

              // TODO: DPad
              bool up = (pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
              bool down = (pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
              bool left = (pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
              bool right = (pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

              new_controller.is_analog = true;
              new_controller.start_x = old_controller.end_x;
              new_controller.start_y = old_controller.end_y;

              // TODO: deadzone processing
              // TODO: min/max macros
              float x;
              if (pad->sThumbLX < 0) {
                x = pad->sThumbLX / 32768;
              } else {
                x = pad->sThumbLX / 32767;
              }
              new_controller.min_x = new_controller.max_x =
                  new_controller.end_x = x;

              float y;
              if (pad->sThumbLY < 0) {
                y = pad->sThumbLY / 32768;
              } else {
                y = pad->sThumbLY / 32767;
              }
              new_controller.min_y = new_controller.max_y =
                  new_controller.end_y = y;

              Win32ProcessXInputDigitalButton(pad->wButtons,
                                              &old_controller.down,
                                              XINPUT_GAMEPAD_A,
                                              &new_controller.down);
              Win32ProcessXInputDigitalButton(pad->wButtons,
                                              &old_controller.right,
                                              XINPUT_GAMEPAD_B,
                                              &new_controller.right);
              Win32ProcessXInputDigitalButton(pad->wButtons,
                                              &old_controller.left,
                                              XINPUT_GAMEPAD_X,
                                              &new_controller.left);
              Win32ProcessXInputDigitalButton(pad->wButtons,
                                              &old_controller.up,
                                              XINPUT_GAMEPAD_Y,
                                              &new_controller.up);
              Win32ProcessXInputDigitalButton(pad->wButtons,
                                              &old_controller.left_shoulder,
                                              XINPUT_GAMEPAD_LEFT_SHOULDER,
                                              &new_controller.left_shoulder);
              Win32ProcessXInputDigitalButton(pad->wButtons,
                                              &old_controller.right_shoulder,
                                              XINPUT_GAMEPAD_RIGHT_SHOULDER,
                                              &new_controller.right_shoulder);
              // bool start = pad->wButtons & XINPUT_GAMEPAD_START;
              // bool back = pad->wButtons & XINPUT_GAMEPAD_BACK;

            } else {
              // NOTE: controller is not available
            }
          }

          DWORD byte_to_lock;
          DWORD bytes_to_write;
          DWORD play_cursor;
          DWORD write_cursor;
          DWORD target_cursor;
          bool sound_valid = false;
          if (SUCCEEDED(secondary_buffer->GetCurrentPosition(&play_cursor,
                                                             &write_cursor))) {
            byte_to_lock =
                (sound_output.sample_index * sound_output.bytes_per_sample) %
                sound_output.secondary_buffer_size;
            target_cursor = (play_cursor + sound_output.latency_sample_count *
                                               sound_output.bytes_per_sample) %
                            sound_output.secondary_buffer_size;
            if (byte_to_lock > target_cursor) {
              bytes_to_write = sound_output.secondary_buffer_size -
                               byte_to_lock + target_cursor;
            } else {
              bytes_to_write = target_cursor - byte_to_lock;
            }

            sound_valid = true;
          }

          GameSoundOutputBuffer sound_buffer = {};
          sound_buffer.samples_per_second = sound_output.samples_per_second;
          sound_buffer.sample_count =
              bytes_to_write / sound_output.bytes_per_sample;
          sound_buffer.samples = samples;

          GameOffscreenBuffer buffer = {};
          buffer.memory = global_buffer.memory;
          buffer.width = global_buffer.width;
          buffer.height = global_buffer.height;
          buffer.pitch = global_buffer.pitch;

          GameUpdateAndRender(&game_memory, new_input, &buffer, &sound_buffer);

          // NOTE: DirectSound output test
          if (sound_valid) {
            Win32FillSoundBuffer(
                &sound_output, byte_to_lock, bytes_to_write, &sound_buffer);
          }

          Win32WindowDimension dimension = Win32GetWindowDimension(window);
          Win32DisplayBufferInWindow(&global_buffer,
                                     device_context,
                                     dimension.width,
                                     dimension.height);

          LARGE_INTEGER end_counter;
          QueryPerformanceCounter(&end_counter);
          int64_t counter_elapsed =
              end_counter.QuadPart - last_counter.QuadPart;
          float ms_per_frame = 1000.0f * counter_elapsed / perf_count_frequency;
          float fps = (float)perf_count_frequency / counter_elapsed;

          uint64_t end_cycle_count = rdtsc();
          uint64_t cycle_elapsed = end_cycle_count - last_cycle_count;
          float micro_cycle_per_frame = cycle_elapsed / (1000.0f * 1000.0f);

          std::cout << std::fixed << std::setprecision(2) << ms_per_frame
                    << "ms, " << fps << "FPS, " << micro_cycle_per_frame
                    << "mc/f" << std::endl;

          last_counter = end_counter;
          last_cycle_count = end_cycle_count;

          GameInput* temp = old_input;
          old_input = new_input;
          new_input = temp;
        }
      } else {
        // TODO: Logging
      }
    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }
  return 0;
}
