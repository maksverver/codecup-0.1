#ifndef PIECE_H_INCLUDED
#define PIECE_H_INCLUDED

#include <cstdint>

enum piece_t : uint8_t {
    WAZIR       = 0,  // 0.1
    KNIGHT      = 1,  // 1.2
    FERZ        = 2,  // 1.1
    DABBABA     = 3,  // 0.2
    ALFIL       = 4,  // 2.2
    PIECE_COUNT = 5,
};

constexpr int piece_counts[PIECE_COUNT] = { 1, 1, 2, 4, 8 };

#endif  // ndef PIECE_H_INCLUDED
