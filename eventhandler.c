#include "eventhandler.h"

// Size of the static preallocated array used to track windows in the event loop
// The array holds 8bit ptrs, which means 64 bytes are used on the stack
// this will likely never be a problem on a system using a screensaver (windows default stack size is 1MB).
// If the app for whatever reason has more then 64 windows (aka 64 monitors) this value can be increased
#define MAX_WINDOWS_PER_EVENTLOOP 64

/**
 * Repaint the full window based on the window state
*/
void RepaintWindow(HWND hwnd, WindowState *windowState) {
  PAINTSTRUCT ps;
  // Create paint handler device context
  HDC hdc = BeginPaint(hwnd, &ps);
  // Create memory paint handler device context as buffer between SRC -> state->bitmapHdc & DST -> hdc
  HDC memDC = CreateCompatibleDC(hdc);
  // Create compatible bitmap and add it as canvas to the memDC
  HBITMAP memBitmap = CreateCompatibleBitmap(hdc, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top);
  HBITMAP oldmap = SelectObject(memDC, memBitmap);

  // Acquire shared lock
  AcquireSRWLockShared(&windowState->lock);

  // Fill memory paint handler device with the background brush
  // When erasing the rect this color is removed, therefore we need to fill the whole rcPaint now
  FillRect(memDC, &ps.rcPaint, windowState->backgroundBrush);

  // Process all images on the window and draw them to the memDC
  for (int i = 0; i < windowState->imageCount; i++) {
    ImageState* imageState = windowState->images[i];
    // Move the bitmap to the memDC (removing transparent color)
    // Draw the bitmap to the rcPaint rect canvas (boundary is set by subtracting left / top of the rcPaint from xPos / yPos)
    // Here we essentially draw the bitmap (state->bitmapHdc) to the full screen (memDC) on a memory device
    TransparentBlt(
      memDC, 
      imageState->xPos - ps.rcPaint.left, imageState->yPos - ps.rcPaint.top,
      imageState->bitmap.bmWidth, imageState->bitmap.bmHeight, 
      imageState->bitmapHdc, 0, 0, imageState->bitmap.bmWidth, imageState->bitmap.bmHeight, windowState->transparentColor
    );
  }

  // Release shared lock
  ReleaseSRWLockShared(&windowState->lock);

  // Move the memDC (redrawn screen) one to one to the hdc using the boundaries of the rcPaint
  BitBlt(
    hdc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left, ps.rcPaint.bottom - ps.rcPaint.top,
    memDC, 0, 0, SRCCOPY
  );

  // Select old Bitmap as canvas, replacing the bitmap here does remove the device context from the original bitmap handle
  // Without this the original bitmap handle would be in a undefined state as it is still associated with a memDC which is deleted
  SelectObject(memDC, oldmap);
  DeleteObject(memBitmap);
  DeleteObject(memDC);

  EndPaint(hwnd, &ps);
}

/**
 * Callback called from the message queue if an event is triggered
 * 
 * Middlewares specified messages 
 * (e.g. to exit the programm on keypress) and gives control back to the default window procedure
*/
LRESULT CALLBACK CallEventHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) {
  // Get userdata (image state ptr) from the handle
  WindowState *windowState = (WindowState*)GetWindowLongPtr(hwnd, GWLP_USERDATA);
  if (!windowState) return DefWindowProc(hwnd, message, wParam, lParam);

  // Static windowStateStack + Counter, tracking all active windows.
  // Tracking is performed on the stack for simplicity, 
  // as it is extremely unlikely that dynamic allocation will be necessary in the future.
  // In this context, each window consistently corresponds to one monitor in a screensaver application.
  static int windowStateStackCount = 0;
  // Array is essentially a stack, holding all windowStates
  // When calling CallCloseWindowLoop() on a windowState the entry should be set to NULL
  static WindowState* windowStateStack[MAX_WINDOWS_PER_EVENTLOOP];
  switch (message) {
    case WM_INITSTATE:
      // Add the state to the windowStateStack and increment the count
      windowStateStack[windowStateStackCount++] = windowState;
      // Acquire unique lock
      AcquireSRWLockExclusive(&windowState->lock);
      // Retrieve cursor position when window is created
      GetCursorPos(windowState->initCursorPosition);
      // Release unique lock
      ReleaseSRWLockExclusive(&windowState->lock);
      
      break;

    case WM_INVALIDATE_RECT:
      // Mark full window as dirty
      // This will trigger a repaint which itself will draw all images on the window at once
      InvalidateRect(hwnd, NULL, FALSE);
      break;

    case WM_PAINT:
      // Repaint the full window (includeing all images)
      RepaintWindow(hwnd, windowState);
      return FALSE;

    case WM_LBUTTONDOWN: // Left mouse click
    case WM_RBUTTONDOWN: // Right mouse click
    case WM_KEYDOWN: // Any key click // Any mouse movement
      // Iterate over all windowStates and close them up
      for (int i = 0; i < windowStateStackCount; i++) {
        if (windowStateStack[i]) {
          // Call the close operation of the window loop, this will async make the window loop close itself
          // After closing itself, the window loop posts a WM_EXIT message which initiates the cleanup of the Window
          CallCloseWindowLoop(windowStateStack[i]);
          windowStateStack[i] = NULL;
        }
      }
      break;

    case WM_MOUSEMOVE:
      // Get cursor position
      POINT point;
      if (!GetCursorPos(&point))
        break;
      
      // Acquire shared lock
      AcquireSRWLockShared(&windowState->lock);

      // Get difference between the current point and the last buffered point
      int diffX = abs(point.x - windowState->initCursorPosition->x);
      int diffY = abs(point.y - windowState->initCursorPosition->y);

      if (diffX > windowState->cursorPositionThreshold || diffY > windowState->cursorPositionThreshold) {
        // Unlock to prevent deadlock, as callCloseWindowLoop will enforce a unique lock
        ReleaseSRWLockShared(&windowState->lock);
        // Iterate over all windowStates and close them up
        for (int i = 0; i < windowStateStackCount; i++) {
          if (windowStateStack[i]) {
            // Call the close operation of the window loop, this will async make the window loop close itself
            // After closing itself, the window loop posts a WM_EXIT message which initiates the cleanup of the Window
            CallCloseWindowLoop(windowStateStack[i]);
            windowStateStack[i] = NULL;
          }
        }
      } else ReleaseSRWLockShared(&windowState->lock);
      break;
    case WM_EXIT:
      // Destroy window states associated window, this will trigger a WM_DESTROY
      // initializing the destruction of the window, at the same time windowState stays in a valid state
      // until it is fully cleaned up in WM_NCDESTROY
      DestroyWindowStateWindow(windowState);
      break;
    case WM_NCDESTROY:
      // Close window state which will release all resources
      CloseWindowState(windowState);
      // This is the last message a window receives, it is triggered through the DestroyWindow() (likely in CloseWindowState)
      // The message will decrement the window count, if on 0 it calls PostQuitMessage to
      if (--windowStateStackCount <= 0) {
        PostQuitMessage(0); 
      }
      break;
  }

  // Continue with default window procedure
  return DefWindowProc(hwnd, message, wParam, lParam);
}