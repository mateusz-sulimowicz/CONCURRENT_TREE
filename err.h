#pragma once

// ERRORCODE: memory allocation failed.
#define EMEMORY -1

// ERRORCODE: trying to move tree to it's own subtree.
// For example: tree_move(/a/, /a/b/)
// should return this errocode.
#define EMOVE -2

/* wypisuje informacje o błędnym zakończeniu funkcji systemowej
i kończy działanie */
extern void syserr(const char* fmt, ...);

/* wypisuje informacje o błędzie i kończy działanie */
extern void fatal(const char* fmt, ...);
