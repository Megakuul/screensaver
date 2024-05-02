#ifndef WINDOWHANDLER_H
#define WINDOWHANDLER_H

#include <windows.h>

#define WM_INVALIDATE_RECT (WM_USER + 1)

/**
 * Represents a single images (bitmap) state
*/
typedef struct {
  // Position of the image
  int xPos;
  int yPos;
  // Current speed of the image
  int xMov;
  int yMov;
  // Current speed addition of the image
  int inc;
  // Base speed addition of the image
  int baseInc;
  // Current speed addition decrementor step
  int decSteps;
  // Base scaler to slow down decrementor
  int baseDecScale;
  // Bitmap metadata
  BITMAP bitmap;
  // Bitmap handler
  HBITMAP bitmapHandler;
  // Bitmap device context
  HDC bitmapHdc;
} ImageState;

/**
 * Create an image state from loaded bitmap resource
 * 
 * If bitmap is not found or the operation fails it returns NULL
*/
ImageState* CreateImageState(
  HINSTANCE instance, 
  int movement, 
  int bounceIncrement, 
  int bounceDecrementScale, 
  int rcBitmapId);

/**
 * Cleans up an image state
 */
void CloseImageState(ImageState *state);

/**
 * Represents one window state
*/
typedef struct {
  // Lock for synchronisation on the window state
  SRWLOCK lock;

  // Bool to indicate exiting the process loop and cleanup the state
  BOOL exitBool;

  // Interval the process loop iterates (in ms)
  double interval;

  // Array of images on the window
  ImageState** images;
  // Count of images on the window
  int imageCount;

  // Reference pointer to initial cursor position, not managed by the struct
  LPPOINT initCursorPosition;
  // Threshold for the cursor position relative to initCursorPosition until a WM_DESTROY message is sent
  int cursorPositionThreshold;

  // Window class to use for the window
  wchar_t* windowClass;
  // Color that is transparented on images
  COLORREF transparentColor;
  // Color brush for background of the window
  HBRUSH backgroundBrush;
  // Window handle
  HWND hwnd;
  // Instance handle
  HINSTANCE hInstance;
} WindowState;

/**
 * Create window state
*/
WindowState* CreateWindowState(
  HINSTANCE hInstance,
  int imageCount, 
  int movementSpeed,
  double interval,
  int bounceIncrement,
  int bounceDecrementScale,
  wchar_t* windowClass, 
  LPRECT monitorRect, 
  LPPOINT initCursorPos, 
  int cursorPositionThreshold,
  int rcBitmapId,
  COLORREF backgroundColor, 
  COLORREF transparentColor);

/**
 * Cleans up window state
 */
void CloseWindowState(WindowState* windowState);

/**
 * Start window processor loop
 * 
 * This function can be run in a seperate thread, the provided window state is synchronized with its lock member.
 * The provided void ptr must be a valid window state.
 *  
 * The loop takes ownership of the window state and cleans it up after calling "CallCloseWindowLoop"
*/
DWORD WINAPI StartWindowLoop(LPVOID lpParam);

/**
 * Triggers a exit of the window process loop and cleanup of the window afterwards
 * 
 * This function can be run in a seperate thread, the provided window state is synchronized with its lock member.
*/
void CallCloseWindowLoop(WindowState* windowState);

#endif