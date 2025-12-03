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

struct OffscreenBuffer {
  BITMAPINFO info;
  void* memory;
  int width;
  int height;
  int pitch;
  int bytes_per_pixel;
};

struct WindowDimension {
  int width;
  int height;
};

struct SoundOutput {
  int samples_per_second;
  int bytes_per_sample;
  int secondary_buffer_size;
  int tone_hz;
  int tone_volume;
  uint32_t sample_index;
  int wave_period;
  float t_sine;
  int latency_sample_count;
};

// TODO: Global variable for now.
global_variable bool running;
global_variable OffscreenBuffer global_buffer;
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

internal void InitDSound(HWND window,
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

internal void FillSoundBuffer(SoundOutput* sound_output,
                              DWORD byte_to_lock,
                              DWORD bytes_to_write) {
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
    int16_t* sample_out = (int16_t*)region1;
    DWORD region1_sample_count = region1_size / sound_output->bytes_per_sample;
    for (DWORD i = 0; i < region1_sample_count; ++i) {
      float sine_value = sinf(2.0 * PI * sound_output->t_sine);
      int16_t sample_value = (int16_t)(sine_value * sound_output->tone_volume);
      *sample_out++ = sample_value;
      *sample_out++ = sample_value;

      sound_output->t_sine += 1.0f / sound_output->wave_period;
      ++sound_output->sample_index;
    }

    sample_out = (int16_t*)region2;
    DWORD region2_sample_count = region2_size / sound_output->bytes_per_sample;
    for (DWORD i = 0; i < region2_sample_count; ++i) {
      float sine_value = sinf(2.0 * PI * sound_output->t_sine);
      int16_t sample_value = (int16_t)(sine_value * sound_output->tone_volume);
      *sample_out++ = sample_value;
      *sample_out++ = sample_value;

      sound_output->t_sine += 1.0f / sound_output->wave_period;
      ++sound_output->sample_index;
    }

    secondary_buffer->Unlock(region1, region1_size, region2, region2_size);
  }
}

internal WindowDimension GetWindowDimension(HWND window) {
  WindowDimension result;

  RECT client_rect;
  GetClientRect(window, &client_rect);
  result.width = client_rect.right - client_rect.left;
  result.height = client_rect.bottom - client_rect.top;

  return result;
}

internal void ResizeDIBSection(OffscreenBuffer* buffer, int width, int height) {
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
  buffer->memory =
      VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}

internal void DisplayBufferInWindow(OffscreenBuffer* buffer,
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

      WindowDimension dimension = GetWindowDimension(window);
      DisplayBufferInWindow(
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

  ResizeDIBSection(&global_buffer, 1280, 720);

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
      int x_offset = 0;
      int y_offset = 0;

      SoundOutput sound_output = {};
      sound_output.samples_per_second = 48000;
      sound_output.bytes_per_sample = sizeof(int16_t) * 2;
      sound_output.secondary_buffer_size =
          sound_output.samples_per_second * sound_output.bytes_per_sample;
      sound_output.latency_sample_count = sound_output.samples_per_second / 15;
      sound_output.tone_hz = 256;  // cycles per second
      sound_output.tone_volume = 3000;
      sound_output.wave_period =
          sound_output.samples_per_second / sound_output.tone_hz;

      InitDSound(window,
                 sound_output.samples_per_second,
                 sound_output.secondary_buffer_size);
      FillSoundBuffer(
          &sound_output,
          0,
          sound_output.latency_sample_count * sound_output.bytes_per_sample);
      secondary_buffer->Play(0, 0, DSBPLAY_LOOPING);

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

        for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) {
          XINPUT_STATE input_state;
          if (XInputGetState(i, &input_state) == ERROR_SUCCESS) {
            // TODO: see if |input_state.dwPacketNumber| increments too rapidly.
            XINPUT_GAMEPAD* pad = &input_state.Gamepad;

            bool up = pad->wButtons & XINPUT_GAMEPAD_DPAD_UP;
            bool down = pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN;
            bool left = pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT;
            bool right = pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT;
            bool start = pad->wButtons & XINPUT_GAMEPAD_START;
            bool back = pad->wButtons & XINPUT_GAMEPAD_BACK;
            bool left_shoulder = pad->wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER;
            bool right_shoulder = pad->wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER;
            bool a_button = pad->wButtons & XINPUT_GAMEPAD_A;
            bool b_button = pad->wButtons & XINPUT_GAMEPAD_B;
            bool x_button = pad->wButtons & XINPUT_GAMEPAD_X;
            bool y_button = pad->wButtons & XINPUT_GAMEPAD_Y;

            int16_t stick_x = pad->sThumbLX;
            int16_t stick_y = pad->sThumbLY;

            x_offset += stick_x / 4096;
            y_offset += stick_y / 4096;

            sound_output.tone_hz = 512 + (int)(256.0f * (stick_y / 30000.0f));
            sound_output.wave_period =
                sound_output.samples_per_second / sound_output.tone_hz;
          } else {
            // NOTE: controller is not available
          }
        }

        GameOffscreenBuffer buffer = {};
        buffer.memory = global_buffer.memory;
        buffer.width = global_buffer.width;
        buffer.height = global_buffer.height;
        buffer.pitch = global_buffer.pitch;
        GameUpdateAndRender(&buffer, x_offset, y_offset);

        // NOTE: DirectSound output test
        DWORD play_cursor;
        DWORD write_cursor;
        if (SUCCEEDED(secondary_buffer->GetCurrentPosition(&play_cursor,
                                                           &write_cursor))) {
          DWORD byte_to_lock =
              (sound_output.sample_index * sound_output.bytes_per_sample) %
              sound_output.secondary_buffer_size;
          DWORD target_cursor =
              (play_cursor + sound_output.latency_sample_count *
                                 sound_output.bytes_per_sample) %
              sound_output.secondary_buffer_size;
          DWORD bytes_to_write;
          if (byte_to_lock > target_cursor) {
            bytes_to_write = sound_output.secondary_buffer_size - byte_to_lock +
                             target_cursor;
          } else {
            bytes_to_write = target_cursor - byte_to_lock;
          }

          FillSoundBuffer(&sound_output, byte_to_lock, bytes_to_write);
        }

        WindowDimension dimension = GetWindowDimension(window);
        DisplayBufferInWindow(
            &global_buffer, device_context, dimension.width, dimension.height);

        LARGE_INTEGER end_counter;
        QueryPerformanceCounter(&end_counter);
        int64_t counter_elapsed = end_counter.QuadPart - last_counter.QuadPart;
        float ms_per_frame = 1000.0f * counter_elapsed / perf_count_frequency;
        float fps = (float)perf_count_frequency / counter_elapsed;

        uint64_t end_cycle_count = rdtsc();
        uint64_t cycle_elapsed = end_cycle_count - last_cycle_count;
        float micro_cycle_per_frame = cycle_elapsed / (1000.0f * 1000.0f);

        std::cout << std::fixed << std::setprecision(2) << ms_per_frame
                  << "ms, " << fps << "FPS, " << micro_cycle_per_frame << "mc/f"
                  << std::endl;

        last_counter = end_counter;
        last_cycle_count = end_cycle_count;
      }
    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }
  return 0;
}
