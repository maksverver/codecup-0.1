#ifndef WEIGHTS_H_INCLUDED
#define WEIGHTS_H_INCLUDED

#include "pieces.h"

// Note: technically we can skip the wazir, since if the game is not over yet,
// then both sides have 1 wazir, so they cancel out.
extern int piece_values[PIECE_COUNT];

// Call this from main() immediately after ParseOptions to initialize pieces
// from options.
void InitializeWeights();

#endif  // ndef WEIGHTS_H_INCLUDED
