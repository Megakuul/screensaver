#ifndef PARSER_H
#define PARSER_H

#include <windows.h>

/**
 * Parse first argument of used on the programm
 * 
 * Options are:
 * - /s -> s is set to true
 * - /p -> p is set to the respective handler
 * - /c -> c is set to true
*/
void ParseConsoleArgument(LPSTR arg, BOOL* s, HWND* p, BOOL* c);

#endif