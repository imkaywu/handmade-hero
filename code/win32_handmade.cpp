#include <windows.h>

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {
  MessageBoxW(0, L"This is Handmade Hero.", L"Handmade Hero",
             MB_OK | MB_ICONINFORMATION);
  return 0;
}
