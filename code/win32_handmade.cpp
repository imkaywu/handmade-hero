#include <stdint.h>
#include <windows.h>

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

internal void DisplayBufferInWindow(HDC device_context,
                                    int window_width,
                                    int window_height,
                                    OffscreenBuffer* buffer,
                                    int x,
                                    int y,
                                    int width,
                                    int height) {
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
    case WM_PAINT: {
      PAINTSTRUCT paint;
      HDC device_context = BeginPaint(window, &paint);
      int x = paint.rcPaint.left;
      int y = paint.rcPaint.top;
      int width = paint.rcPaint.right - paint.rcPaint.left;
      int height = paint.rcPaint.bottom - paint.rcPaint.top;

      WindowDimension dimension = GetWindowDimension(window);
      DisplayBufferInWindow(device_context,
                            dimension.width,
                            dimension.height,
                            &buffer,
                            x,
                            y,
                            width,
                            height);

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

      while (running) {
        while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            running = false;
          }
          TranslateMessage(&message);
          DispatchMessage(&message);
        }

        RenderWeirdGradient(&buffer, x_offset, y_offset);

        WindowDimension dimension = GetWindowDimension(window);
        DisplayBufferInWindow(device_context,
                              dimension.width,
                              dimension.height,
                              &buffer,
                              0,
                              0,
                              dimension.width,
                              dimension.height);

        x_offset += 1;
        y_offset += 1;
      }
    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }
  return 0;
}
