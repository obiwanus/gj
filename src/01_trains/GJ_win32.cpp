// ============================ Program code ==================================

#include <intrin.h>
#include "../GJ_base.h"
#include "../common/GJ_math.h"

#include "game.h"

#include "game.cpp"

// ========================== Platform headers ================================

#include <windows.h>
#include <windowsX.h>
#include <gl/gl.h>

// =========================== Platform code ==================================

static void Win32ResizeClientWindow(HWND window) {
  if (!g_pixel_buffer.memory) return;  // no buffer yet

  RECT client_rect;
  GetClientRect(window, &client_rect);
  int width = client_rect.right - client_rect.left;
  int height = client_rect.bottom - client_rect.top;

  if (width > g_pixel_buffer.max_width) {
    width = g_pixel_buffer.max_width;
  }
  if (height > g_pixel_buffer.max_height) {
    height = g_pixel_buffer.max_height;
  }

  g_pixel_buffer.width = width;
  g_pixel_buffer.height = height;
  g_pixel_buffer.was_resized = true;
}

LRESULT CALLBACK
Win32WindowProc(HWND Window, UINT uMsg, WPARAM wParam, LPARAM lParam) {
  LRESULT Result = 0;

  switch (uMsg) {
    case WM_SIZE: {
      Win32ResizeClientWindow(Window);
    } break;

    case WM_CLOSE: {
      g_running = false;
    } break;

    case WM_PAINT: {
      PAINTSTRUCT Paint = {};
      HDC hdc = BeginPaint(Window, &Paint);
      Win32UpdateWindow(hdc);
      EndPaint(Window, &Paint);
    } break;

    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
    case WM_KEYDOWN:
    case WM_KEYUP: {
      assert(!"Keyboard input came in through a non-dispatch message!");
    } break;

    default: { Result = DefWindowProc(Window, uMsg, wParam, lParam); } break;
  }

  return Result;
}

inline LARGE_INTEGER Win32GetWallClock() {
  LARGE_INTEGER Result;
  QueryPerformanceCounter(&Result);

  return Result;
}

inline r32 Win32GetMillisecondsElapsed(LARGE_INTEGER Start, LARGE_INTEGER End) {
  r32 Result = 1000.0f * (r32)(End.QuadPart - Start.QuadPart) /
               (r32)gPerformanceFrequency.QuadPart;

  return Result;
}

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
                     LPSTR lpCmdLine, int nCmdShow) {
  // Allocate program memory
  g_program_memory.start = malloc(MAX_INTERNAL_MEMORY_SIZE);
  g_program_memory.free_memory = g_program_memory.start;
  // TODO: add checks for overflow when allocating

  // Main program state
  Program_State *state =
      (Program_State *)g_program_memory.allocate(sizeof(Program_State));
  state->init(&g_program_memory, &g_pixel_buffer,
              (Raytrace_Work_Queue *)&g_raytrace_queue);

  // Create window class
  WNDCLASS WindowClass = {};
  WindowClass.style = CS_OWNDC | CS_VREDRAW | CS_HREDRAW;
  WindowClass.lpfnWndProc = Win32WindowProc;
  WindowClass.hInstance = hInstance;
  WindowClass.lpszClassName = "VMWindowClass";

  // Set target sleep resolution
  {
    TIMECAPS tc;
    UINT wTimerRes;

    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
      OutputDebugStringA("Cannot set the sleep resolution\n");
      exit(1);
    }

    wTimerRes = min(max(tc.wPeriodMin, 1), tc.wPeriodMax);  // 1 ms
    timeBeginPeriod(wTimerRes);
  }

  QueryPerformanceFrequency(&gPerformanceFrequency);

  if (!RegisterClass(&WindowClass)) {
    // TODO: logging
    printf("Couldn't register window class\n");
    exit(1);
  }

  // Create window so that its client area is exactly kWindowWidth/Height
  DWORD WindowStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
  RECT WindowRect = {};

  WindowRect.right = state->kWindowWidth;
  WindowRect.bottom = state->kWindowHeight;
  AdjustWindowRect(&WindowRect, WindowStyle, 0);
  int WindowWidth = WindowRect.right - WindowRect.left;
  int WindowHeight = WindowRect.bottom - WindowRect.top;
  HWND Window = CreateWindow(WindowClass.lpszClassName, 0, WindowStyle,
                             CW_USEDEFAULT, CW_USEDEFAULT, WindowWidth,
                             WindowHeight, 0, 0, hInstance, 0);

  if (!Window) {
    printf("Couldn't create window\n");
    exit(1);
  }

  // We're not going to release it as we use CS_OWNDC
  HDC hdc = GetDC(Window);

  g_running = true;

  // Set proper buffer values based on actual client size
  Win32ResizeClientWindow(Window);

  // Init OpenGL
  {
    PIXELFORMATDESCRIPTOR DesiredPixelFormat = {};
    DesiredPixelFormat.nSize = sizeof(DesiredPixelFormat);
    DesiredPixelFormat.nVersion = 1;
    DesiredPixelFormat.iPixelType = PFD_TYPE_RGBA;
    DesiredPixelFormat.dwFlags =
        PFD_SUPPORT_OPENGL | PFD_DRAW_TO_WINDOW | PFD_DOUBLEBUFFER;
    DesiredPixelFormat.cColorBits = 32;
    DesiredPixelFormat.cAlphaBits = 8;
    DesiredPixelFormat.iLayerType = PFD_MAIN_PLANE;

    int SuggestedPixelFormatIndex = ChoosePixelFormat(hdc, &DesiredPixelFormat);
    PIXELFORMATDESCRIPTOR SuggestedPixelFormat;
    DescribePixelFormat(hdc, SuggestedPixelFormatIndex,
                        sizeof(SuggestedPixelFormat), &SuggestedPixelFormat);

    SetPixelFormat(hdc, SuggestedPixelFormatIndex, &SuggestedPixelFormat);

    HGLRC OpenGLRC = wglCreateContext(hdc);
    if (wglMakeCurrent(hdc, OpenGLRC)) {
      // Success
      glGenTextures(1, &gTextureHandle);

      typedef BOOL WINAPI wgl_swap_interval_ext(int interval);
      wgl_swap_interval_ext *wglSwapInterval =
          (wgl_swap_interval_ext *)wglGetProcAddress("wglSwapIntervalEXT");
      if (wglSwapInterval) {
        wglSwapInterval(1);
      } else {
        // VSync not enabled or not supported
        assert(false);
      }
    } else {
      // Something's wrong
      assert(false);
    }
  }

  Cursor_Type current_cursor = Cursor_Type_Arrow;
  HCURSOR win_cursors[Cursor_Type__COUNT];
  win_cursors[Cursor_Type_Arrow] = LoadCursor(NULL, IDC_ARROW);
  win_cursors[Cursor_Type_Cross] = LoadCursor(NULL, IDC_CROSS);
  win_cursors[Cursor_Type_Hand] = LoadCursor(NULL, IDC_HAND);
  win_cursors[Cursor_Type_Resize_Vert] = LoadCursor(NULL, IDC_SIZENS);
  win_cursors[Cursor_Type_Resize_Horiz] = LoadCursor(NULL, IDC_SIZEWE);

  User_Input inputs[2];
  User_Input *old_input = &inputs[0];
  User_Input *new_input = &inputs[1];
  *new_input = {};

  LARGE_INTEGER last_timestamp = Win32GetWallClock();

   // Event loop
  while (g_running) {
    // Process messages
    MSG message;
    while (PeekMessage(&message, 0, 0, 0, PM_REMOVE)) {
      // Get keyboard messages
      switch (message.message) {
        case WM_QUIT: {
          g_running = false;
        } break;

        case WM_SYSKEYDOWN:
        case WM_SYSKEYUP:
        case WM_KEYDOWN:
        case WM_KEYUP: {
          u32 vk_code = (u32)message.wParam;
          bool was_down = ((message.lParam & (1 << 30)) != 0);
          bool is_down = ((message.lParam & (1 << 31)) == 0);

          bool alt_key_was_down = (message.lParam & (1 << 29)) != 0;
          if ((vk_code == VK_F4) && alt_key_was_down) {
            g_running = false;
          }
          if (was_down == is_down) {
            break;  // nothing has changed
          }
          if (vk_code == VK_ESCAPE) {
            new_input->buttons[IB_escape] = is_down;
          }
          if (vk_code == VK_UP) {
            new_input->buttons[IB_up] = is_down;
          }
          if (vk_code == VK_DOWN) {
            new_input->buttons[IB_down] = is_down;
          }
          if (vk_code == VK_LEFT) {
            new_input->buttons[IB_left] = is_down;
          }
          if (vk_code == VK_RIGHT) {
            new_input->buttons[IB_right] = is_down;
          }
          if (vk_code == VK_SHIFT) {
            new_input->buttons[IB_shift] = is_down;
          }

          // Handle symbols
          int symbol = (int)vk_code;
          if (vk_code == VK_NUMPAD0)
            symbol = '0';
          else if (vk_code == VK_NUMPAD1)
            symbol = '1';
          else if (vk_code == VK_NUMPAD2)
            symbol = '2';
          else if (vk_code == VK_NUMPAD3)
            symbol = '3';
          else if (vk_code == VK_NUMPAD4)
            symbol = '4';
          else if (vk_code == VK_NUMPAD5)
            symbol = '5';
          else if (vk_code == VK_NUMPAD6)
            symbol = '6';
          else if (vk_code == VK_NUMPAD7)
            symbol = '7';
          else if (vk_code == VK_NUMPAD8)
            symbol = '8';
          else if (vk_code == VK_NUMPAD9)
            symbol = '9';

          if (('A' <= symbol && symbol <= 'Z') ||
              ('0' <= symbol && symbol <= '9')) {
            new_input->buttons[IB_key] = is_down;
            new_input->symbol = symbol;
          }
        } break;

        case WM_MOUSEWHEEL: {
          int delta = (int)message.wParam / (65536 * WHEEL_DELTA);
          new_input->scroll += delta;
        } break;

        default: {
          TranslateMessage(&message);
          DispatchMessageA(&message);
        } break;
      }
    }

    // Get mouse input
    {
      POINT mouse_pointer;
      GetCursorPos(&mouse_pointer);
      ScreenToClient(Window, &mouse_pointer);

      new_input->mouse = {mouse_pointer.x, mouse_pointer.y};

      new_input->buttons[IB_mouse_left] =
          (GetKeyState(VK_LBUTTON) & (1 << 15)) != 0;
      new_input->buttons[IB_mouse_right] =
          (GetKeyState(VK_RBUTTON) & (1 << 15)) != 0;
      new_input->buttons[IB_mouse_middle] =
          (GetKeyState(VK_MBUTTON) & (1 << 15)) != 0;
    }

    Update_Result result =
        update_and_render(&g_program_memory, state, new_input);

    Win32UpdateWindow(hdc);

    // Swap inputs
    User_Input *tmp = old_input;
    old_input = new_input;
    new_input = tmp;

    // Zero input
    *new_input = {};
    new_input->old = old_input;  // Save so we can refer to it later

    // Retain the button state
    for (size_t i = 0; i < COUNT_OF(new_input->buttons); i++) {
      new_input->buttons[i] = old_input->buttons[i];
    }
    for (size_t i = 0; i < COUNT_OF(new_input->mouse_positions); i++) {
      new_input->mouse_positions[i] = old_input->mouse_positions[i];
    }
    new_input->symbol = old_input->symbol;

    r32 ms_elapsed =
        Win32GetMillisecondsElapsed(last_timestamp, Win32GetWallClock());
    g_FPS.value = (int)(1000.0f / ms_elapsed);
    last_timestamp = Win32GetWallClock();
  }

  return 0;
}
