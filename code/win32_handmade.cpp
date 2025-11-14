#include <windows.h>

#define internal static
#define local_persist static
#define global_variable static

// TODO: Global variable for now.
global_variable bool running;

global_variable BITMAPINFO bitmap_info;
global_variable void *bitmap_memory;
global_variable HBITMAP bitmap_handle;
global_variable HDC bitmap_device_context;

internal void ResizeDIBSection(int width, int height) {
  // TODO: Bulletbroof freeing DIBSection.
  // TODO: Maybe not free first, free after, then free first if that fails
  if (bitmap_handle) {
    DeleteObject(bitmap_handle);
  }

  if (!bitmap_device_context) {
    bitmap_device_context = CreateCompatibleDC(0);
  }

  bitmap_info.bmiHeader.biSize = sizeof(bitmap_info.bmiHeader);
  bitmap_info.bmiHeader.biWidth = width;
  bitmap_info.bmiHeader.biHeight = height;
  bitmap_info.bmiHeader.biPlanes = 1;
  bitmap_info.bmiHeader.biBitCount = 32;
  bitmap_info.bmiHeader.biCompression = BI_RGB;

  bitmap_handle = CreateDIBSection(bitmap_device_context,
                                   &bitmap_info,
                                   DIB_RGB_COLORS,
                                   &bitmap_memory,
                                   0,
                                   0);

  ReleaseDC(0, bitmap_device_context);
}

internal void
UpdateWindow(HDC device_context, int x, int y, int width, int height) {
  StretchDIBits(device_context,
                x,
                y,
                width,
                height,
                x,
                y,
                width,
                height,
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
      UpdateWindow(device_context, x, y, width, height);
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
    HWND window_handle = CreateWindowExA(0,
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

    if (window_handle) {
      MSG message;
      running = true;
      while (running) {
        BOOL message_result = GetMessage(&message, 0, 0, 0);
        if (message_result > 0) {
          TranslateMessage(&message);
          DispatchMessage(&message);
        } else {
          break;
        }
      }
    } else {
      // TODO: Logging
    }
  } else {
    // TODO: Logging
  }
  return 0;
}
