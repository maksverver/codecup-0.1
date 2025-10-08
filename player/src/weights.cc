#include "weights.h"

#include "options.h"

DECLARE_OPTION(int, arg_knight,  1, "knight",   "Piece value of a knight");
DECLARE_OPTION(int, arg_ferz,    1, "ferz",     "Piece value of a ferz");
DECLARE_OPTION(int, arg_dabbaba, 1, "dabbaba",  "Piece value of a dabbaba");
DECLARE_OPTION(int, arg_alfil,   1, "alfil",    "Piece value of a alfil");

void InitializeWeights() {
    piece_values[KNIGHT]  = arg_knight;
    piece_values[FERZ]    = arg_ferz;
    piece_values[DABBABA] = arg_dabbaba;
    piece_values[ALFIL]   = arg_alfil;
}

int piece_values[PIECE_COUNT] {
    100,  // 1x Wazir    (0.1)
      1,  // 1x Knight   (1.2)
      1,  // 2x Ferz     (1.1)
      1,  // 4x Dabbaba  (0.2)
      1,  // 8x Alfil    (2.2)
};
