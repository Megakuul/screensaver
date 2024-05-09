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
  double relativeImageWidth,
  BOOL disableImageScale,
  int imageId,
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
  
  // Create window
  if (hWindow==NULL) {
    // If no window handle is provided, create a new window with the size of the monitorRect
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
      NULL                                    // Additional arguments
    );
  } else {
    // If a window handle is provided, create a child window with the size of the parent
    // We don't directly use the provided window handle, because we need the window
    // to have the windowClass in order to correctly function with the core eventloop
    RECT rect;
    if (GetWindowRect(hWindow, &rect)) {
      HWND hwndChild = CreateWindowEx(
        0,                      // No extended styles
        windowClass,            // Your custom window class
        L"",                    // Window title
        WS_CHILD | WS_VISIBLE,  // Window style
        0,                      // X position relative to parent
        0,                      // Y position relative to parent
        rect.right - rect.left, // Width
        rect.bottom - rect.top, // Height
        hWindow,                // Parent window
        NULL,                   // Menu
        windowState->hInstance, // Instance handle
        NULL                    // Additional arguments
      );
      windowState->hwnd = hwndChild;
    }
  }
  if (!windowState->hwnd) return NULL;

  // Acquire created window rect
  RECT windowRect;
  if (!GetWindowRect(windowState->hwnd, &windowRect)) return NULL;

  // Set absolute image width to the relativeImageWidth * window size
  int absoluteImageWidth = relativeImageWidth * (windowRect.right - windowRect.left);

  // Allocate image state array
  windowState->images = malloc(sizeof(ImageState*) * imageCount);
  if (!windowState->images) return NULL;

  // Create all image states and add them to the array
  windowState->imageCount = imageCount;
  for (int i = 0; i < windowState->imageCount; i++) {
    windowState->images[i] = 
      CreateImageState(
        windowState->hInstance,
        windowRect,
        movementSpeed,
        bounceIncrement,
        bounceDecrementScale, 
        absoluteImageWidth,
        disableImageScale,
        imageId
      );
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
  RECT windowRect,
  int movement, 
  int bounceIncrement, 
  double bounceDecrementScale,
  int imageWidth,
  BOOL disableImageScale,
  int imageId) {

  // Load bitmap handle
  HBITMAP origBitmapHandle = LoadBitmap((HINSTANCE)instance, MAKEINTRESOURCE(imageId));
  if (!origBitmapHandle) return NULL;
  // Create temporary bitmap object
  BITMAP origBitmap = (BITMAP){0};
  // Load image data into the origBitmap object
  GetObject(origBitmapHandle, sizeof(origBitmap), &origBitmap);
  // Create device context for original bitmap image
  HDC origBitmapHdc = CreateCompatibleDC(NULL);
  // Select bitmap handle to the device context
  HBITMAP oldBitmapHandle = SelectObject(origBitmapHdc, origBitmapHandle);

  // Scale is based on the imageWidth provided, that way the WindowState 
  // can calculate a size of the image based on the size of the window
  int scaledWidth = imageWidth;
  // The height is calculated by obtaining the scale factor of the width and then applying it to the original height
  int scaledHeight = ((double)imageWidth / (double)origBitmap.bmWidth) * origBitmap.bmHeight;

  // Create image state object
  ImageState *imageState = malloc(sizeof(ImageState));
  if (!imageState) return NULL;

  // Set image state attributes
  imageState->xPos = 5 + (rand() % (windowRect.right - windowRect.left - scaledWidth - 10)); // Rand start position (+ 5 pixel border)
  imageState->yPos = 5 + (rand() % (windowRect.bottom - windowRect.top - scaledHeight - 10)); // Rand start position (+ 5 pixel border)
  imageState->xMov = movement * (rand() % 2 ? -1 : 1); // Random move start direction
  imageState->yMov = movement * (rand() % 2 ? -1 : 1); // Random move start direction
  imageState->inc = 0;
  imageState->decSteps = 1;
  imageState->baseInc = bounceIncrement;
  imageState->baseDecScale = bounceDecrementScale;
  InitializeSRWLock(&imageState->lock);

  // If image scale is disabled, use the original handle, hdc and bitmap
  if (disableImageScale) {
    imageState->bitmapHdc = origBitmapHdc;
    imageState->bitmapHandle = origBitmapHandle;
    imageState->bitmap = origBitmap;
    imageState->oldBitmapHandle = oldBitmapHandle;

    return imageState;
  }

  // Create scaled bitmap handle
  imageState->bitmapHandle = CreateCompatibleBitmap(GetDC(NULL), scaledWidth, scaledHeight);
  // Create scaled device context
  imageState->bitmapHdc = CreateCompatibleDC(NULL);
  // Select scaled bitmap handle to the device context
  imageState->oldBitmapHandle = SelectObject(imageState->bitmapHdc, imageState->bitmapHandle);

  // Use the halftone so that the scaled image does not look absolutely horrible
  SetStretchBltMode(imageState->bitmapHdc, HALFTONE);
  SetBrushOrgEx(imageState->bitmapHdc, 0, 0, NULL);  // Necessary for HALFTONE

  // Draw and scale the original image to the device context
  StretchBlt(
    imageState->bitmapHdc, 0, 0, scaledWidth, scaledHeight,
    origBitmapHdc, 0, 0, origBitmap.bmWidth, origBitmap.bmHeight, SRCCOPY
  );

  // Create bitmap object
  imageState->bitmap = (BITMAP){0};
  // Load scaled image data into the bitmap object
  GetObject(imageState->bitmapHandle, sizeof(imageState->bitmap), &imageState->bitmap);

  // Unselect original bitmap handle
  SelectObject(origBitmapHdc, oldBitmapHandle);
  // Cleanup original bitmap handle
  DeleteObject(origBitmapHandle);
  // Cleanup original bitmap hdc
  DeleteDC(origBitmapHdc);

  return imageState;
}

/**
 * Cleans up an image state and its associated resources
 * 
 * This function must be called from the thread the imageState was created on!
 */
void CloseImageState(ImageState *imageState) {
  if (imageState) {
    // Unselect bitmap handle
    SelectObject(imageState->bitmapHdc, imageState->oldBitmapHandle);
    // Cleanup bitmap handle
    DeleteObject(imageState->bitmapHandle);
    // Cleanup bitmap hdc
    DeleteDC(imageState->bitmapHdc);
    free(imageState);
  }
}

/**
 * Insertion sort based on the x position of the image state
*/
void insertionSort(ImageState* imageStates[], int imageStatesLength) {
  int i, j;
  ImageState* key;
  for (i = 0; i < imageStatesLength; i++) {
    key = imageStates[i];
    j = i - 1;

    // Reverse iterate and move elements up as long as they are larger then the key
    while (j >= 0 && imageStates[j]->xPos > key->xPos) {
      // Move current element (imageStates[j]) one element up
      imageStates[j + 1] = imageStates[j];
      j--;
    }
    // Set the last element which was moved up to the key
    imageStates[j + 1] = key;
  }
}

void resolveCollision(ImageState* objectA, ImageState* objectB) {

  // Calculate the overlap of the two objects
  int overlapX = (objectA->xPos + objectA->bitmap.bmWidth) - objectB->xPos;
  int overlapY = (objectA->yPos + objectA->bitmap.bmHeight) - objectB->yPos;

  // Add half of the overlap to both objects so that they are not colliding anymore
  objectA->xPos -= (overlapX / 2) + 1;
  objectB->xPos += (overlapX / 2) + 1;
  objectA->yPos -= (overlapY / 2) + 1;
  objectB->yPos += (overlapY / 2) + 1;

  // Change movement direction
  objectA->xMov = - objectA->xMov;
  objectA->yMov = - objectA->yMov;
  objectB->xMov = - objectB->xMov;
  objectB->yMov = - objectB->yMov;
}

/**
 * Checks for collisions on the images and updates their movement appropriately
 * 
 * Sweep & prune like algorithm is used to check for collisions
 * the main axis is sorted with insertion sort because it is very efficient for "almost-sorted" lists
 * 
 * Algorithm is not very efficient but omits O(n^2) average speed by using the sorted list to "split" the collision detection into sections
*/
void HandleCollisions(ImageState* imageStates[], int imageStatesLength) {
  insertionSort(imageStates, imageStatesLength);
  for (int i = 0; i < imageStatesLength; i++) {
    int localLeft = imageStates[i]->xPos;
    int localRight = imageStates[i]->xPos + imageStates[i]->bitmap.bmWidth;
    int localTop = imageStates[i]->yPos;
    int localBottom = imageStates[i]->yPos+imageStates[i]->bitmap.bmHeight;

    for (int j = i + 1; j < imageStatesLength; j++) {
      int remoteLeft = imageStates[j]->xPos;
      int remoteRight = imageStates[j]->xPos + imageStates[j]->bitmap.bmWidth;
      int remoteTop = imageStates[j]->yPos;
      int remoteBottom = imageStates[j]->yPos+imageStates[j]->bitmap.bmHeight;

      if (localRight < remoteLeft) break;

      if (localBottom >= remoteTop && localTop <= remoteBottom) {
        resolveCollision(imageStates[i], imageStates[j]);
      }
    }
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

    // Handle image collisions
    HandleCollisions(windowState->images, windowState->imageCount);

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