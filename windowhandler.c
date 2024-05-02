#include "windowhandler.h"

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
  COLORREF transparentColor) {

  WindowState* windowState = malloc(sizeof(WindowState));
  if (!windowState) return NULL;

  windowState->hInstance = hInstance;
  windowState->windowClass = windowClass;
  windowState->initCursorPosition = initCursorPos;
  windowState->exitBool = FALSE;
  InitializeSRWLock(&windowState->lock);

  windowState->backgroundBrush = CreateSolidBrush(backgroundColor);
  windowState->transparentColor = transparentColor;
  windowState->interval = interval;
  windowState->cursorPositionThreshold = cursorPositionThreshold;
  
  windowState->hwnd = CreateWindowEx(
    0,                                      // Extended window style
    windowClass,                            // Window class name
    L"",                                    // Window title
    WS_POPUP | WS_VISIBLE,                  // Window style
    monitorRect->left,                      // X position
    monitorRect->top,                       // Y position
    monitorRect->right - monitorRect->left, // Width
    monitorRect->bottom - monitorRect->top, // Height
    NULL,                                   // Parent window   
    NULL,                                   // Menu
    windowState->hInstance,                 // Instance handle
    windowState    
  );
  if (!windowState->hwnd) return NULL;

  windowState->images = malloc(sizeof(ImageState*) * imageCount);
  if (!windowState->images) return NULL;

  windowState->imageCount = imageCount;
  for (int i = 0; i < windowState->imageCount; i++) {
    windowState->images[i] = 
      CreateImageState(windowState->hInstance, movementSpeed, bounceIncrement, bounceDecrementScale, rcBitmapId);
  }

  ShowWindow(windowState->hwnd, SW_SHOW);
  UpdateWindow(windowState->hwnd);

  return windowState;
}

/**
 * Triggers a exit of the window process loop and cleanup of the window afterwards
*/
void CallCloseWindowLoop(WindowState* windowState) {
  AcquireSRWLockExclusive(&windowState->lock);
  windowState->exitBool = TRUE;
  ReleaseSRWLockExclusive(&windowState->lock);
}

/**
 * Cleans up window state
 */
void CloseWindowState(WindowState* windowState) {
  if (windowState) {
    DeleteObject(windowState->backgroundBrush);
    DestroyWindow(windowState->hwnd);
    for (int i = 0; i < windowState->imageCount; i++) {
      CloseImageState(windowState->images[i]);
    }
    free(windowState->images);
    free(windowState);
  }
}

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
  int rcBitmapId) {
  HBITMAP bitmapHandler = LoadBitmap((HINSTANCE)instance, MAKEINTRESOURCE(rcBitmapId));
  if (!bitmapHandler) return NULL;

  // Create image state object
  ImageState *imageState = malloc(sizeof(ImageState));
  if (!imageState) return NULL;

  // Set image state attributes
  imageState->xPos = 0;
  imageState->yPos = 0;
  imageState->xMov = movement;
  imageState->yMov = movement;
  imageState->inc = 0;
  imageState->decSteps = 1;
  imageState->baseInc = bounceIncrement;
  imageState->baseDecScale = bounceDecrementScale;

  imageState->bitmap = (BITMAP){0};

  // Set image handler and get metadata
  imageState->bitmapHandler = bitmapHandler;
  GetObject(imageState->bitmapHandler, sizeof(imageState->bitmap), &imageState->bitmap);

  // Create bitmap hdc
  imageState->bitmapHdc = CreateCompatibleDC(NULL);
  SelectObject(imageState->bitmapHdc, bitmapHandler);

  return imageState;
}

/**
 * Cleans up an image state
 */
void CloseImageState(ImageState *state) {
  if (state) {
    DeleteObject(state->bitmapHandler);
    DeleteDC(state->bitmapHdc);
    free(state);
  }
}

/**
 * Updates the position of the provided image state
 * 
 * When colliding with the handler window it will bounce of with a logarithmic-decreasing boost
*/
void UpdateImagePosition(HWND hwnd, ImageState *state) {
  // Get client rect
  RECT clientRect;
  GetClientRect(hwnd, &clientRect);

  if (state->inc>0) {
    // Decrement incrementor with a logarithmic decrementation
    // Logarithm the difference between log(a) and log(a+1) gets smaller when a is getting larger
    // therefore its quite good to create a smooth animation, because if a (aka decStep) is small
    // it won't decrement much from inc
    state->inc *= state->baseDecScale * (log(state->decSteps + 1) / log(state->decSteps + 2));
    // Decrement decrement steps
    state->decSteps++;
    // Check boundaries
    state->inc = state->inc<=0 ? 0 : state->inc;
  }

  // Convert the incrementors operator in the operator of the current move state
  int xInc = (state->xMov >= 0) ? state->inc : -state->inc;
  int yInc = (state->yMov >= 0) ? state->inc : -state->inc;

  // Move image
  state->xPos += state->xMov + xInc;
  state->yPos += state->yMov + yInc;

  // Check if position in bound, if not movement is inverted
  if (state->xPos + state->bitmap.bmWidth > clientRect.right || state->xPos < clientRect.left) {
    state->inc = state->baseInc;
    state->decSteps = 1;
    state->xMov = - state->xMov;
  }
  // Check if position in bound, if not movement is inverted
  if (state->yPos + state->bitmap.bmHeight > clientRect.bottom || state->yPos < clientRect.top) {
    state->inc = state->baseInc;
    state->decSteps = 1;
    state->yMov = - state->yMov;
  }
}

/**
 * Start window processor loop
 * 
 * This function can be run in a seperate thread.
 * The provided void ptr must be a valid window state.
 * To close the loop and clean up the window
*/
DWORD WINAPI StartWindowLoop(LPVOID lpParam) {
  WindowState* windowState = (WindowState*)lpParam;

  // Initialize parameters used for the high precision timer
  LARGE_INTEGER start, freq, now;
  QueryPerformanceFrequency(&freq);
  QueryPerformanceCounter(&start);

  double elapsed = 0.0;
 
  while (TRUE) {
    // Acquire unique lock
    AcquireSRWLockExclusive(&windowState->lock);
    // Run loop until exitBool is set
    if (windowState->exitBool) {
      ReleaseSRWLockExclusive(&windowState->lock);
      break;
    }

    QueryPerformanceCounter(&now);
    elapsed = (double)(now.QuadPart - start.QuadPart) / freq.QuadPart;

    if (elapsed >= windowState->interval) {
      QueryPerformanceCounter(&start);

      if (elapsed > windowState->interval)
        start.QuadPart -= (LONGLONG)((elapsed - windowState->interval) * freq.QuadPart);
    }

    // Rerender and calculate the position of all images on the window
    for (int i = 0; i < windowState->imageCount; i++) {
      UpdateImagePosition(windowState->hwnd, windowState->images[i]);
    }
    // PostMessage is calling the Windows UI system message queue and is thread-safe
    PostMessage(windowState->hwnd, WM_INVALIDATE_RECT, 0, 0);

    double leftover = windowState->interval - elapsed;
    // Release unique lock
    ReleaseSRWLockExclusive(&windowState->lock);

    // If leftover is under 0.002 omit the Sleep syscall as this will take more time then is leftover
    if (leftover > 0.002) {
      Sleep(leftover);
    }
  }
  CloseWindowState(windowState);
  return 0;
}