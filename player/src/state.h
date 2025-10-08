#ifndef STATE_H_INCLUDED
#define STATE_H_INCLUDED

#include "pieces.h"
#include "random.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <limits>
#include <optional>
#include <random>
#include <span>
#include <string>
#include <string_view>
#include <vector>

enum color_t : uint8_t {
    RED         = 0,
    BLUE        = 1,
    COLOR_COUNT = 2,
};

inline color_t Other(color_t color) { return static_cast<color_t>(color ^ 1); }

// Encodes the piece on a field:
//
//  0000000 empty
//  00pppc1 occupied by color c piece p,
//
using field_t = uint8_t;

constexpr int ROW_COUNT   = 8;
constexpr int COL_COUNT   = 8;
constexpr int FIELD_COUNT = ROW_COUNT * COL_COUNT;

constexpr field_t EMPTY_FIELD = 0;

inline field_t Field(color_t color, piece_t piece) {
    return (piece << 2) | (color << 1) | 1;
}

inline bool IsEmpty(field_t f) { return f == 0; }
inline bool HasColor(field_t f, color_t c) { return (f &  3) == ((c << 1) | 1); };
inline bool HasPiece(field_t f, piece_t p) { return (f & 29) == ((p << 2) | 1); };
inline color_t Color(field_t f) { return static_cast<color_t>((f >> 1) & 1); }
inline piece_t Piece(field_t f) { return static_cast<piece_t>((f >> 2) & 7); }

struct State {
    // Pieces on the board (64 bytes)
    field_t fields[FIELD_COUNT];

    // Captured pieces on hand (10 bytes; might be reduced to 8)
    uint8_t captured[2][PIECE_COUNT];

    // 0-based turn index; indicates next player (4 bytes; can be reduced to 1)
    int turn;

    //
    // Derived data follows
    //

    // Scores used for evaluation (8 bytes, can be reduced)
    int scores[2];

    // Returns the initial state.
    inline static State Initial() { return State{}; }

    // Initializes a state from the given arguments, and correctly initializes
    // derived fields like `scores`.
    static State Create(field_t fields[FIELD_COUNT], uint8_t captured[2][PIECE_COUNT], int turn);

    field_t &FieldAt(int row, int col) {
        return fields[(row << 3) | col];
    }

    const field_t &FieldAt(int row, int col) const {
        return fields[(row << 3) | col];
    }

    color_t NextPlayer() const {
        return static_cast<color_t>(turn & 1);
    }

    bool GameOver() const {
        return captured[RED][WAZIR] || captured[BLUE][WAZIR];
    }

    bool HasWon(color_t color) const {
        return captured[color][WAZIR];
    }

    color_t Winner() const {
        if (HasWon(RED))  return RED;
        if (HasWon(BLUE)) return BLUE;
        return COLOR_COUNT;
    }

    // Can be optimized by omitting `scores`.
    auto operator<=>(const State&) const = default;
};


inline uint8_t FieldIndex(uint8_t row, uint8_t col) { return (row << 3) | col; }
inline uint8_t Row(uint8_t i) { return i >> 3; }
inline uint8_t Col(uint8_t i) { return i & 7; }
inline bool InBounds(int r, int c) { return 0 <= r && r < 8 && 0 <= c && c < 8; }

// Encodes a move during normal play.
//
// If src < 64, it indicates a move from field src to field dst.
//
// If src >= 64, then it indicates deploying a captured piece of type (src - 64)
// at field dst.
//
struct Move {
    uint8_t src;
    uint8_t dst;

    auto operator<=>(const Move&) const = default;

    bool IsNull() const { return src == 0 && dst == 0; }

    static Move Null() { return {}; }
};

// Encodes a move that sets up one player's pieces.
//
// For the first player (red), the pieces go into fields 0 through 15,
// and for the second player (blue), the pieces go into fields 48 through 47.
struct SetupMove {
    std::array<piece_t, 16> pieces;

    auto operator<=>(const SetupMove&) const = default;
};

struct UndoState {
    uint8_t src;
    uint8_t dst;
    piece_t old_piece;
};

// Executes the move in the given state (the move MUST be valid!)
UndoState ExecuteMove(State &state, const Move &move);

void UndoMove(State &state, const UndoState &undo);

// Executes the setup move in the given state (the setup move MUST be valid!)
void ExecuteSetupMove(State &state, const SetupMove &setup_move);

// Maximum number of moves in any position (see ../docs/max-moves.txt for details.)
constexpr size_t MAX_MOVES = 256;

// Generates moves into the given `moves` buffer, and returns the number of moves.
size_t GenerateAllMoves(const State &state, Move (&moves)[MAX_MOVES]);

// Same as GenerateAllMoves() above, but copies the result into a vector.
// Easier to use in places where maximal performance doesn't matter.
std::vector<Move> GenerateAllMoves(const State &state);

// I/O support

inline int ParseRow(char ch) { return ch >= 'a' && ch <= 'h' ? ch - 'a' : -1; }
inline int ParseCol(char ch) { return ch >= '1' && ch <= '8' ? ch - '1' : -1; }

inline char FormatRow(int row) { return row >= 0 && row < 8 ? 'a' + row : '?'; }
inline char FormatCol(int col) { return col >= 0 && col < 8 ? '1' + col : '?'; }

std::optional<std::pair<color_t, piece_t>> ParseColoredPiece(char ch);
char FormatColoredPiece(color_t color, piece_t piece);

std::optional<Move> ParseMove(color_t color, std::string_view s);
std::optional<SetupMove> ParseSetupMove(color_t color, std::string_view s);

std::string FormatMove(color_t color, const Move &move);
std::string FormatSetupMove(color_t color, const SetupMove &setup_move);

void DebugPrint(std::ostream &os, const State &state);

// Details exported for use in analysis.cc

struct Delta2D { int8_t dr, dc; };

extern const Delta2D piece_delta_data[24];

extern const std::span<const Delta2D> piece_delta[PIECE_COUNT];

#endif // ndef STATE_H_INCLUDED
