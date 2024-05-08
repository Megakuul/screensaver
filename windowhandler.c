#include "windowhandler.h"

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
  COLORREF transparentColor) {

  WindowState* windowState = malloc(sizeof(WindowState));
  if (!windowState) return NULL;

  windowState->hInstance = hInstance;
  windowState->windowClass = windowClass;
  windowState->initCursorPosition = initCursorPos;
  InitializeSRWLock(&windowState->initCursorPositionLock);
  windowState->exitBool = FALSE;

  windowState->backgroundBrush = CreateSolidBrush(backgroundColor);
  windowState->transparentColor = transparentColor;
  windowState->interval = interval;
  windowState->cursorPositionThreshold = cursorPositionThreshold;
  
  if (hWindow==NULL) {
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
      NULL    
    );
    if (!windowState->hwnd) return NULL;
  } else windowState->hwnd = hWindow;

  windowState->images = malloc(sizeof(ImageState*) * imageCount);
  if (!windowState->images) return NULL;

  windowState->imageCount = imageCount;
  for (int i = 0; i < windowState->imageCount; i++) {
    windowState->images[i] = 
      CreateImageState(windowState->hInstance, movementSpeed, bounceIncrement, bounceDecrementScale, rcBitmapId);
  }

  SetWindowLongPtr(windowState->hwnd, GWLP_USERDATA, (LONG_PTR)windowState);

  // Start initialization of the window
  PostMessage(windowState->hwnd, WM_INITSTATE, 0, 0);

  ShowWindow(windowState->hwnd, SW_SHOW);
  UpdateWindow(windowState->hwnd);

  return windowState;
}

/**
 * Destroys windowState's associated Window
 * 
 * This effectively doesn't affect windowState but destroys the window leading to a WM_DESTROY message beeing sent
 * 
 * Useful to start the cleanup process of the window
 * 
 * This function must be called from the thread the windowState was created on!
 */
void DestroyWindowStateWindow(WindowState* windowState) {
  if (windowState && windowState->hwnd) {
    HWND hwnd = windowState->hwnd;
    windowState->hwnd = NULL;
    DestroyWindow(hwnd);
  }
}

/**
 * Cleans up window state and its associated resources
 * 
 * This function must be called from the thread the windowState was created on!
 */
void CloseWindowState(WindowState* windowState) {
  if (windowState) {
    DeleteObject(windowState->backgroundBrush);
    if (windowState->hwnd) {
      HWND hwnd = windowState->hwnd;
      windowState->hwnd = NULL;
      DestroyWindow(hwnd);
    }
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
  double bounceDecrementScale, 
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
  InitializeSRWLock(&imageState->lock);

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
 * Cleans up an image state and its associated resources
 * 
 * This function must be called from the thread the imageState was created on!
 */
void CloseImageState(ImageState *imageState) {
  if (imageState) {
    DeleteObject(imageState->bitmapHandler);
    DeleteDC(imageState->bitmapHdc);
    free(imageState);
  }
}

/**
 * Updates the position of the provided image state
 * 
 * When colliding with the handler window it will bounce of with a logarithmic-decreasing boost
 * 
 * This function is synchronizing updates to mutable components of the image state with a unique lock
*/
void UpdateImagePosition(HWND hwnd, ImageState *imageState) {
  // If handle is not valid anymore, skip it
  if (!hwnd) return;
  
  // Get client rect
  RECT clientRect;
  GetClientRect(hwnd, &clientRect);

  // Acquire unique lock
  AcquireSRWLockExclusive(&imageState->lock);

  if (imageState->inc>0) {
    // Decrement incrementor with a logarithmic decrementation
    // Logarithm the difference between log(a) and log(a+1) gets smaller when a is getting larger
    // therefore its quite good to create a smooth animation, because if a (aka decStep) is small
    // it won't decrement much from inc
    imageState->inc *= imageState->baseDecScale * (log(imageState->decSteps + 1) / log(imageState->decSteps + 2));
    // Decrement decrement steps
    imageState->decSteps++;
    // Check boundaries
    imageState->inc = imageState->inc<=0 ? 0 : imageState->inc;
  }

  // Convert the incrementors operator in the operator of the current move state
  int xInc = (imageState->xMov >= 0) ? imageState->inc : -imageState->inc;
  int yInc = (imageState->yMov >= 0) ? imageState->inc : -imageState->inc;

  // Move image
  imageState->xPos += imageState->xMov + xInc;
  imageState->yPos += imageState->yMov + yInc;

  // Check if position in bound, if not movement is inverted
  if (imageState->xPos + imageState->bitmap.bmWidth > clientRect.right || imageState->xPos < clientRect.left) {
    imageState->inc = imageState->baseInc;
    imageState->decSteps = 1;
    imageState->xMov = - imageState->xMov;
  }
  // Check if position in bound, if not movement is inverted
  if (imageState->yPos + imageState->bitmap.bmHeight > clientRect.bottom || imageState->yPos < clientRect.top) {
    imageState->inc = imageState->baseInc;
    imageState->decSteps = 1;
    imageState->yMov = - imageState->yMov;
  }

  // Release unique lock
  ReleaseSRWLockExclusive(&imageState->lock);
}

/**
 * Start window processor loop
 * 
 * This function can be run in a seperate thread, the provided window state is synchronized with its lock member.
 * The provided void ptr must be a valid window state.
 * 
 * Messages are sent to the eventloop "listening" on the thread the WindowState was created!
*/
DWORD WINAPI StartWindowLoop(LPVOID lpParam) {
  WindowState* windowState = (WindowState*)lpParam;

  // Initialize parameters used for the high precision timer
  LARGE_INTEGER start, freq, now;
  // Acquire frequency (ticks per second) 
  QueryPerformanceFrequency(&freq);
  // Acquire ticks since system start
  QueryPerformanceCounter(&start);

  double elapsed = 0.0;
 
  while (TRUE) {
    // Run loop until exitBool is set
    if (InterlockedCompareExchange(&windowState->exitBool, FALSE, FALSE)) {
      break;
    }

    // Acquire ticks since system start
    QueryPerformanceCounter(&now);

    // Rerender and calculate the position of all images on the window
    for (int i = 0; i < windowState->imageCount; i++) {
      UpdateImagePosition(windowState->hwnd, windowState->images[i]);
    }

    // PostMessage is calling the Windows UI system message queue and is thread-safe
    PostMessage(windowState->hwnd, WM_INVALIDATE_RECT, 0, 0);
    
    // Calculate tick difference between now and the start buffer and convert it to seconds by dividing by frequency
    elapsed = ((double)(now.QuadPart - start.QuadPart) / freq.QuadPart) * 1000;

    // The windows Sleep() syscall is extremly unprecise and can cause weird behavior, like rounding down / ignoring 
    // low intervals (like <20ms). Therefore we use a busy-spinner here, which essentially spins an empty loop
    // until the time elapsed. In order to not fully starve the cpu, we use a Sleep(0) yielding control to the kernel, which can 
    // then send this thread sleeping for 1 tick if there are other busy or more important threads.
    while(elapsed < windowState->interval) {
      // Yield control to kernel for 0-1 ticks
      Sleep(0);
      // Take elapsed snapshot for comparison
      QueryPerformanceCounter(&now);
      elapsed = ((double)(now.QuadPart - start.QuadPart) / freq.QuadPart) * 1000;
    }

    // Reset start counter
    QueryPerformanceCounter(&start);
  }
  // Send an exit message to the eventloop
  PostMessage(windowState->hwnd, WM_EXIT, 0, 0);
  return 0;
}

/**
 * Triggers a exit of the window process loop which will close the loop and send a WM_EXIT message to the eventloop
 * 
 * Operation sets exitBool to true but doesn't guarantee 
*/
void CallCloseWindowLoop(WindowState* windowState) {
  InterlockedExchange(&windowState->exitBool, TRUE);
}