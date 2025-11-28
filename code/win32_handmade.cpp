#include <dsound.h>
#include <stdint.h>
#include <windows.h>
#include <xinput.h>

#include <iostream>

#define internal static
#define local_persist static
#define global_variable static

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

// TODO: Global variable for now.
global_variable bool running;
global_variable OffscreenBuffer buffer;

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

      LPDIRECTSOUNDBUFFER secondary_buffer;
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

internal WindowDimension GetWindowDimension(HWND window) {
  WindowDimension result;

  RECT client_rect;
  GetClientRect(window, &client_rect);
  result.width = client_rect.right - client_rect.left;
  result.height = client_rect.bottom - client_rect.top;

  return result;
}

internal void RenderWeirdGradient(OffscreenBuffer* buffer,
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
          &buffer, device_context, dimension.width, dimension.height);

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
  LoadXInput();

  WNDCLASSA window_class = {};
  window_class.style = CS_OWNDC | CS_HREDRAW | CS_VREDRAW;
  window_class.lpfnWndProc = MainWindowCallback;
  window_class.hInstance = instance;
  window_class.lpszClassName = "HandmadeHeroWindowClass";

  ResizeDIBSection(&buffer, 1280, 720);

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
      running = true;
      int x_offset = 0;
      int y_offset = 0;

      InitDSound(window, 48000, 48000 * sizeof(int16_t) * 2);

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

            if (a_button) {
              y_offset += 1;
            }
          } else {
            // NOTE: controller is not available
          }
        }

        XINPUT_VIBRATION vibration;
        vibration.wLeftMotorSpeed = 60000;
        vibration.wRightMotorSpeed = 60000;
        XInputSetState(0, &vibration);

        RenderWeirdGradient(&buffer, x_offset, y_offset);

        WindowDimension dimension = GetWindowDimension(window);
        DisplayBufferInWindow(
            &buffer, device_context, dimension.width, dimension.height);

        x_offset += 1;
      }
    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }
  return 0;
}
