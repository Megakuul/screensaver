#include "parser.h"

/**
 * Parse first argument of used on the programm
 * 
 * Options are:
 * - /s -> s is set to true
 * - /p -> p is set to the respective handler
 * - /c -> c is set to true
*/
void ParseConsoleArgument(LPSTR arg, BOOL* s, HWND* p, BOOL* c) {
  // Default initialize values
  *s = FALSE;
  *c = FALSE;
  *p = NULL;

  char* nexttok = NULL;
  // Start tokenize the arg string
  char* tok = strtok_s(arg, " ", &nexttok);
  while (tok != NULL) {
    // If token is /s set s to true
    if (strcmp(tok, "/s") == 0) *s = TRUE;
    // If token is /c set c to true
    else if (strcmp(tok, "/c") == 0) *c = TRUE;
    // If token is /p set p to the next arg by reading the next token
    else if (strcmp(tok, "/p") == 0 && (tok = strtok_s(NULL, " ", &nexttok)) != NULL) {
      // Convert token to unsigned long and cast to window handler
      uintptr_t hwnd = strtoull(tok, NULL, 0);
      *p = (HWND)hwnd;
    }
    // Read next token from arg
    tok = strtok_s(NULL, " ", &nexttok); 
  }
}