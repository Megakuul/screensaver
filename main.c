#include <windows.h>

#include "parser.h"
#include "eventhandler.h"
#include "windowhandler.h"

// Defines the BITMAP ID to identify the loaded bitmap
#define IDB_LOGOBITMAP 101

// Defines transparent color of the screensaver
#define IDB_LOGOBITMAP_TRANSPARENT_COLOR RGB(255, 255, 255)

// Defines background color of the screensaver
#define BACKGROUND_COLOR RGB(240, 240, 240)

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
   * Default update interval in ms. This value should be set to 1000 / the displays refresh rate for optimal movement
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
 * Acquires the value for the specified registry key as double
 * 
 * If the registry entry is not found or can't be converted to double, it returns the specified default value
*/
double getRegDouble(HKEY root, LPCWSTR sub, LPCWSTR val, DWORD type, double def) {
  HKEY key;
  double result = def;
  wchar_t data[255];
  DWORD dataSize = sizeof(data);

  // Open regkey
  if (RegOpenKeyEx(root, sub, 0, KEY_READ, &key) == ERROR_SUCCESS) {
    // Get value entry as
    if (RegGetValue(key, NULL, val, RRF_RT_ANY, &type, data, &dataSize) == ERROR_SUCCESS) {
      
      // Convert data into a double (aka longfloat)
      wchar_t* end;
      double convertedValue = wcstod(data, &end);
      if (end != data) {
        result = convertedValue;
      }
      // If conversion fails, use default value
    }
    RegCloseKey(key);
  }
  return result;
}

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
  if (GetMonitorInfo(hMonitor, (LPMONITORINFO)&mi)) {
    // Fetch displayFrequency from the monitor info data
    DEVMODE dm;
    dm.dmSize = sizeof(DEVMODE);
    if (EnumDisplaySettings(mi.szDevice, ENUM_CURRENT_SETTINGS, &dm)) {
      // Set interval to the display frequency to synchronize frames with movement updates
      request->interval = 1000 / dm.dmDisplayFrequency;
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
    .cursorThreshold = getRegDouble(HKEY_CURRENT_USER, L"Software\\screensaver", L"cursor_threshold", REG_SZ, 20),
    .count = getRegDouble(HKEY_CURRENT_USER, L"Software\\screensaver", L"image_count", REG_SZ, 1),
    .interval = 1000 / 60, // Default to 60hz
    .speed = getRegDouble(HKEY_CURRENT_USER, L"Software\\screensaver", L"image_speed", REG_SZ, 1),
    .bounce = getRegDouble(HKEY_CURRENT_USER, L"Software\\screensaver", L"image_bounce", REG_SZ, 20),
    .bounceScale = getRegDouble(HKEY_CURRENT_USER, L"Software\\screensaver", L"image_bounce_scale", REG_SZ, 0.7),
    .bitmap = IDB_LOGOBITMAP,
    .backgroundColor = BACKGROUND_COLOR,
    .transparentColor = IDB_LOGOBITMAP_TRANSPARENT_COLOR
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
