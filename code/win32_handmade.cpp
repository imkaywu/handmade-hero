#include <stdint.h>
#include <windows.h>

#define internal static
#define local_persist static
#define global_variable static

// TODO: Global variable for now.
global_variable bool running;

global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable int bitmap_width;
global_variable int bitmap_height;
global_variable int bytes_per_pixel = 4;

internal void RenderWeirdGradient(int x_offset, int y_offset) {
  int width = bitmap_width;
  int height = bitmap_height;

  int pitch = width * bytes_per_pixel;
  uint8_t *row = (uint8_t *)bitmap_memory;
  for (int y = 0; y < bitmap_height; ++y) {
    uint32_t *pixel = (uint32_t *)row;
    for (int x = 0; x < bitmap_width; ++x) {
      // LITTLE ENDIAN FORMAT
      // pixel in memory: BB GG RR xx
      // pixel in register: xx RR GG BB
      uint8_t blue = (x + x_offset);
      uint8_t green = (y + y_offset);

      *pixel++ = ((green << 8) | blue);
    }

    row += pitch;
  }
}

internal void ResizeDIBSection(int width, int height) {
  // TODO: Bulletbroof freeing DIBSection.
  // TODO: Maybe not free first, free after, then free first if that fails
  if (bitmap_memory) {
    VirtualFree(bitmap_memory, 0, MEM_RELEASE);
  }

  bitmap_width = width;
  bitmap_height = height;

  bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
  bitmap_info.bmiHeader.biWidth = bitmap_width;
  bitmap_info.bmiHeader.biHeight = -bitmap_height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  int bitmap_memory_size = bitmap_width * bitmap_height * bytes_per_pixel;
  bitmap_memory =
      VirtualAlloc(0, bitmap_memory_size, MEM_COMMIT, PAGE_READWRITE);
}

internal void UpdateWindow(HDC device_context,
                           RECT *client_rect,
                           int x,
                           int y,
                           int width,
                           int height) {
  int client_width = client_rect->right - client_rect->left;
  int client_height = client_rect->bottom - client_rect->top;
  StretchDIBits(device_context,
                0,
                0,
                client_width,
                client_height,
                0,
                0,
                bitmap_width,
                bitmap_height,
                bitmap_memory,
                &bitmap_info,
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
      RECT client_rect;
      GetClientRect(window, &client_rect);
      int width = client_rect.right - client_rect.left;
      int height = client_rect.bottom - client_rect.top;
      ResizeDIBSection(width, height);
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

      RECT client_rect;
      GetClientRect(window, &client_rect);
      UpdateWindow(device_context, &client_rect, x, y, width, height);

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
      MSG message;
      running = true;
      int x_offset = 0;
      int y_offset = 0;

      while (running) {
        if (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
          if (message.message == WM_QUIT) {
            running = false;
          }
          TranslateMessage(&message);
          DispatchMessage(&message);
        }

        RenderWeirdGradient(x_offset, y_offset);

        HDC device_context = GetDC(window);
        RECT client_rect;
        GetClientRect(window, &client_rect);
        int client_width = client_rect.right - client_rect.left;
        int client_height = client_rect.bottom - client_rect.top;
        UpdateWindow(
            device_context, &client_rect, 0, 0, client_width, client_height);
        ReleaseDC(window, device_context);

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
