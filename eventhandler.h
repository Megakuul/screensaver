#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H

#include <windows.h>

#include "windowhandler.h"

/**
 * Callback called from the message queue if an event is triggered
 * 
 * Middlewares specified messages 
 * (e.g. to exit the programm on keypress) and gives control back to the default window procedure
 * 
 * Expects the windows to have a valid windowState pointer in the GWLP_USERDATA
 * - If the pointer on the window is NULL it will immediately call the DefWindowProc
 * - If the pointer exists but is no WindowState* this leads to undefined behavior
 * 
 * WindowStates provided on the window handles are expected to be created in the same
 * thread as the eventhandler is processed
*/
LRESULT CALLBACK CallEventHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif