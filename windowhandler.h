#ifndef WINDOWHANDLER_H
#define WINDOWHANDLER_H

#include <windows.h>

#define WM_INITSTATE (WM_USER + 1)
#define WM_INVALIDATE_RECT (WM_USER + 2)
#define WM_EXIT (WM_USER + 3)

/**
 * Represents a single images (bitmap) state
*/
typedef struct {
  // Mutex lock to synchronize access to the image state
  SRWLOCK lock;
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
  double baseDecScale;
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
  double bounceDecrementScale, 
  int rcBitmapId);

/**
 * Cleans up an image state and its associated resources
 * 
 * This function must be called from the thread the imageState was created on!
 */
void CloseImageState(ImageState *state);

/**
 * Represents one window state
*/
typedef struct {
  // Bool to indicate exiting the process loop
  volatile LONG exitBool;

  // Interval the process loop iterates (in ms)
  double interval;

  // Array of images on the window
  ImageState** images;
  // Count of images on the window
  int imageCount;

  // Reference pointer to initial cursor position, not managed by the struct
  LPPOINT initCursorPosition;
  // Lock for synchronisation on initCursorPosition
  SRWLOCK initCursorPositionLock;
  // Threshold for the cursor position relative to initCursorPosition until a WM_DESTROY message is sent
  int cursorPositionThreshold;

  // Window class to use for the window
  wchar_t* windowClass;
  // Color that is transparented on images
  COLORREF transparentColor;
  // Color brush for background of the window
  HBRUSH backgroundBrush;
  // Window handle of the associated window
  // This handle is set to NULL upon destruction of the window to handle the destruction gracefully (not leading to undefined behavior)
  HWND hwnd;
  // Instance handle
  HINSTANCE hInstance;
} WindowState;

/**
 * Create window state
 * 
 * Set hWindow to NULL to create a window
 * 
 * This function must be called in the thread where you expect the WM_EXIT/WM_DESTROY messages in the eventloop,
 * the reason for this is that windows "binds" the created window to the thread it was created in
*/
WindowState* CreateWindowState(
  HINSTANCE hInstance,
  HWND hWindow,
  int imageCount, 
  int movementSpeed,
  double interval,
  int bounceIncrement,
  double bounceDecrementScale,
  wchar_t* windowClass, 
  LPRECT monitorRect, 
  LPPOINT initCursorPos, 
  int cursorPositionThreshold,
  int rcBitmapId,
  COLORREF backgroundColor, 
  COLORREF transparentColor);


/**
 * Destroys windowState's associated Window
 * 
 * This effectively doesn't affect windowState but destroys the window leading to a WM_DESTROY message beeing sent
 * 
 * Useful to start the cleanup process of the window
 * 
 * This function must be called from the thread the windowState was created on!
 */
void DestroyWindowStateWindow(WindowState* windowState);

/**
 * Cleans up window state and its associated resources
 * 
 * This function must be called from the thread the windowState was created on!
 */
void CloseWindowState(WindowState* windowState);

/**
 * Start window processor loop
 * 
 * This function can be run in a seperate thread, the provided window state is synchronized with its lock member.
 * The provided void ptr must be a valid window state.
*/
DWORD WINAPI StartWindowLoop(LPVOID lpParam);

/**
 * Triggers a exit of the window process loop which will close the loop and send a WM_EXIT message to the eventloop
 * 
 * This function can be run in a seperate thread, the provided window state is synchronized with its lock member.
*/
void CallCloseWindowLoop(WindowState* windowState);

#endif