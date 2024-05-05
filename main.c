#include <windows.h>

#include "parser.h"
#include "eventhandler.h"
#include "windowhandler.h"

// Color which will be transparent, this is used because
// gdi does not support bitmaps with alpha channel
#define TRANSPARENT_COLOR RGB(255, 255, 255)

// Screensaver background color
#define BACKGROUND_COLOR RGB(240,240,240)

// Movement per update
#define UPDATE_MOVEMENT 2
// Update interval in MS
#define UPDATE_INTERVAL 1

// Speed increment which is applied to the image initially (will decrement in logarithmic steps)
#define BOUNCE_INCREMENTOR 20

// Parameter calculated into the logarithmic decrement of the incrementor, it essentially makes the bounce less aggressive when lowering
#define BOUNCE_DECREMENT_SCALE 0.50

// The pixel threshold the mouse can move (x|y) until the screensaver is quit
#define MOUSE_MOVE_THRESHOLD 20

// Defines the BITMAP ID to identify the loaded bitmap
#define IDB_LOGOBITMAP 101

// Label used for the screen saver window class
#define SCREEN_SAVER_WINDOW_CLASS_LABEL L"MPW_ScreenSaverWindow"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  WNDCLASS wc = {0};
  wc.lpfnWndProc = CallEventHandler;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = SCREEN_SAVER_WINDOW_CLASS_LABEL;

  if (!RegisterClass(&wc)) return FALSE;

  // True if the app should display settings
  BOOL displaySettings = FALSE;
  // True if the app should display the screen saver on every screen
  BOOL displayFull = FALSE;
  // Specifies a Window handle if preview mode is enabled
  HWND previewWindow = NULL;
  // Parse console arguments into options
  ParseConsoleArgument(lpCmdLine, &displayFull, &previewWindow, &displaySettings);
  // There are no settings, so the application is just closed
  if (displaySettings) {
    return FALSE;
  }
  
  // Initial cursor point
  POINT GlobalCursorPoint = { .x = 0, .y = 0 };
  // Hide cursor
  ShowCursor(FALSE);

  // If display Full is set, create a handle on every monitor
  if (displayFull) {
    // Create solid brush and set it to the global background brush
    GlobalBackgroundBrush = CreateSolidBrush(BACKGROUND_COLOR);
    // Iterate over all monitors and create a ScreenSaver window for them
    EnumDisplayMonitors(NULL, NULL, CreateWindowProc, (LPARAM)hInstance);
  } else if (previewWindow) {
    // Load bitmap from embedded resource
    WindowState* pState = CreateImageState(hInstance);
    if (!pState) return FALSE;

    SetWindowLongPtr(previewWindow, GWLP_USERDATA, (LONG_PTR)pState);

    // Create solid brush and set it to the global background brush
    GlobalBackgroundBrush = CreateSolidBrush(BACKGROUND_COLOR);

    // If a specific handle is specified, use this to show the window
    ShowWindow(previewWindow, SW_SHOW);
    UpdateWindow(previewWindow);
  } else {
    // If no flag is specified program exits
    return FALSE;
  }

  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  /**
   * Cleanup of the state in combination with the enumeration of screens, 
   * can cause a slight delay before closing, this could potentially impact the customer
   * therefore in this particular code allocated memory is cleaned up by the OS!
   */
  // EnumWindows(CloseWindowProc, 0);

  // Clean up global background brush
  DeleteObject(GlobalBackgroundBrush);

  return msg.wParam;
}
