#include <windows.h>

#include "parser.h"
#include "eventhandler.h"
#include "windowhandler.h"

// Defines the BITMAP ID to identify the loaded bitmap
#define IDB_LOGOBITMAP 101

/**
 * Request holding "environment" relevant data to create a window
*/
typedef struct {
  /**
   * HINSTANCE of the application
  */
  HINSTANCE hInstance;
  /**
   * Window class for the window
  */
  wchar_t *windowClass;
  /**
   * Reference to initial cursor position
  */
  LPPOINT initCursorPos;
  /**
   * Cursor threshold from initCursorPos until application is exiting
  */
  int cursorThreshold; 
  /**
   * Count of images to spawn
  */
  int count;
  /**
   * Default update interval. This value should be set to the displays refresh rate optimally
  */
  int interval;
  /**
   * Speed of the images in pixels per frame
  */
  int speed;
  /**
   * Instant bounce speed incrementation in pixels
  */
  int bounce;
  /**
   * Bounce decremention scale (makes the bounce decrement less aggressive)
  */
  double bounceScale;
  /**
   * Id of the bitmap resource to load and display
  */
  int bitmap;
  /**
   * Background color of the window
  */
  COLORREF backgroundColor; 
  /**
   * Color which will be removed when drawing to the canvas
  */
  COLORREF transparentColor;
} WindowCreationRequest;


/**
 * Procedure leveraging a provided previewWindow to create a windowState on top of it
*/
BOOL CreatePreviewWindow(HWND hWindow, WindowCreationRequest* request) {
  WindowState* windowState = CreateWindowState(
    request->hInstance,
    hWindow, // Specifing the preview handle will stop it from creating a new window
    request->count,
    request->speed,
    request->interval,
    request->bounce,
    request->bounceScale,
    request->windowClass,
    NULL, // Monitor rect is NULL, because no window must be created
    request->initCursorPos,
    request->cursorThreshold,
    request->bitmap,
    request->backgroundColor,
    request->transparentColor
  );
  if (!windowState) return FALSE;

  // Create windowThread running the WindowProcessLoop
  HANDLE windowThread = CreateThread(
    NULL, // Default security attrs
    0, // Default stack size
    StartWindowLoop,
    (LPVOID)windowState,
    0, // Default creation flags
    NULL // No need to acquire the thread identifier
  );
  // Immediately close the window handle, this just removes the handle and does not interupt the thread (essentially detaching it)
  // The thread is cleaned up automatically by the evenloop which will send a exit signal to stop the loop
  if (windowThread!=NULL) {
    CloseHandle(windowThread);
    return TRUE;
  }
  else return FALSE;
}

/**
 * Procedure called to iterate over monitors, creating a windowState for each
 * 
 * On failure it will return FALSE which stops the enumeration
*/
BOOL CALLBACK CreateMonitorWindow(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
  WindowCreationRequest* request = (WindowCreationRequest*)dwData;
  // Fetch monitor info data
  MONITORINFOEX mi;
  mi.cbSize = sizeof(MONITORINFOEX);
  if (GetMonitorInfo(hMonitor, &mi)) {
    // Fetch displayFrequency from the monitor info data
    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);
    if (EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
      // Set interval to the display frequency to synchronize frames with movement updates
      request->interval = dm.dmDisplayFrequency;
    }
  };

  // Create the window state object using the hWindow=NULL option to create a new window 
  // with the dimensions of the lprcMonitor
  WindowState* windowState = CreateWindowState(
    request->hInstance,
    NULL,
    request->count,
    request->speed,
    request->interval,
    request->bounce,
    request->bounceScale,
    request->windowClass,
    lprcMonitor,
    request->initCursorPos,
    request->cursorThreshold,
    request->bitmap,
    request->backgroundColor,
    request->transparentColor
  );
  if (!windowState) return FALSE;

  // Create windowThread running the WindowProcessLoop
  HANDLE windowThread = CreateThread(
    NULL, // Default security attrs
    0, // Default stack size
    StartWindowLoop,
    (LPVOID)windowState,
    0, // Default creation flags
    NULL // No need to acquire the thread identifier
  );
  // Immediately close the window handle, this just removes the handle and does not interupt the thread (essentially detaching it)
  // The thread is cleaned up automatically by the evenloop which will send a exit signal to stop the loop
  if (windowThread!=NULL) {
    CloseHandle(windowThread);
    return TRUE;
  }
  else return FALSE;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
  WNDCLASS wc = {0};
  wc.lpfnWndProc = CallEventHandler;
  wc.hInstance = hInstance;
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.lpszClassName = L"ScreenSaverWindow";

  if (!RegisterClass(&wc)) return FALSE;

  // Initial cursor point
  POINT initCursorPos = { .x = 0, .y = 0 };

  // Create window creation request
  WindowCreationRequest windowCreationRequest = {
    .hInstance = hInstance,
    .windowClass = L"ScreenSaverWindow",
    .initCursorPos = &initCursorPos,
    .cursorThreshold = 20,
    .count = 3,
    .interval = 60,
    .speed = 2,
    .bounce = 20,
    .bounceScale = 0.7,
    .bitmap = 101,
    .backgroundColor = RGB(240, 240, 240),
    .transparentColor = RGB(255, 255, 255)
  };

  // True if the app should display settings
  BOOL displaySettings = FALSE;
  // True if the app should display the screen saver on every screen
  BOOL displayFull = FALSE;
  // Specifies a Window handle if preview mode is enabled
  HWND hPreviewWindow = NULL;
  // Parse console arguments into options
  ParseConsoleArgument(lpCmdLine, &displayFull, &hPreviewWindow, &displaySettings);
  // There are no settings, so the application is just closed
  if (displaySettings) {
    return FALSE;
  }

  // If display Full is set, create a handle on every monitor
  if (displayFull) {
    // Iterate over all monitors and create a ScreenSaver window for them
    if (!EnumDisplayMonitors(NULL, NULL, CreateMonitorWindow, (LPARAM)&windowCreationRequest)) return FALSE;
  } else if (hPreviewWindow) {
    // Create a ScreenSaver window from the provided window handle
    if (!CreatePreviewWindow(hPreviewWindow, &windowCreationRequest)) return FALSE;
  } else {
    // If no flag is specified program exits
    return FALSE;
  }

  // Hide cursor
  ShowCursor(FALSE);

  // Start main event loop
  MSG msg;
  while (GetMessage(&msg, NULL, 0, 0)) {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  return msg.wParam;
}
