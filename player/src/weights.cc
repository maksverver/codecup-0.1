#include "weights.h"

#include "options.h"

constexpr int default_value_wazir   = 100;
constexpr int default_value_knight  =   9;
constexpr int default_value_ferz    =   1;
constexpr int default_value_dabbaba =   4;
constexpr int default_value_alfil   =   2;

DECLARE_OPTION(int, arg_knight,  default_value_knight,  "knight",   "Piece value of a knight");
DECLARE_OPTION(int, arg_ferz,    default_value_ferz,    "ferz",     "Piece value of a ferz");
DECLARE_OPTION(int, arg_dabbaba, default_value_dabbaba, "dabbaba",  "Piece value of a dabbaba");
DECLARE_OPTION(int, arg_alfil,   default_value_alfil,   "alfil",    "Piece value of a alfil");

void InitializeWeights() {
    piece_values[KNIGHT]  = arg_knight;
    piece_values[FERZ]    = arg_ferz;
    piece_values[DABBABA] = arg_dabbaba;
    piece_values[ALFIL]   = arg_alfil;
}

int piece_values[PIECE_COUNT] {
    default_value_wazir,    // 1x Wazir    (0.1)
    default_value_knight,   // 1x Knight   (1.2)
    default_value_ferz,     // 2x Ferz     (1.1)
    default_value_dabbaba,  // 4x Dabbaba  (0.2)
    default_value_alfil,    // 8x Alfil    (2.2)
};
