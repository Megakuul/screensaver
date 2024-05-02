#ifndef EVENTHANDLER_H
#define EVENTHANDLER_H

#include <windows.h>

#include "windowhandler.h"

/**
 * Callback called from the message queue if an event is triggered
 * 
 * Middlewares specified messages 
 * (e.g. to exit the programm on keypress) and gives control back to the default window procedure
*/
LRESULT CALLBACK CallEventHandler(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam);

#endif