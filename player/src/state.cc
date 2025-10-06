#include "state.h"

#include <cassert>
#include <cstring>
#include <span>
#include <string_view>

namespace {

const std::string_view piece_chars[2] = {
    "WNFDA",
    "wnfda",
};

struct Delta2D { int8_t dr, dc; };

const Delta2D piece_delta_data[24] = {
    // Wazir (0.1)
    { -1,  0 },  //  0
    {  0, -1 },  //  1
    {  0, +1 },  //  2
    { +1,  0 },  //  3
    // Knight (1.2)
    { -2, -1 },  //  4
    { -2, +1 },  //  5
    { -1, -2 },  //  6
    { -1, +2 },  //  7
    { +1, -2 },  //  8
    { +1, +2 },  //  9
    { +2, -1 },  // 10
    { +2, +1 },  // 11
    // Ferz (1.1)
    { -1, -1 },  // 12
    { -1, +1 },  // 13
    { +1, -1 },  // 14
    { +1, +1 },  // 15
    // Dabbaba (0.2)
    { -2,  0 },  // 16
    {  0, -2 },  // 17
    {  0, +2 },  // 18
    { +2,  0 },  // 19
    // Alfil (2.2)
    { -2, -2 },  // 20
    { -2, +2 },  // 21
    { +2, -2 },  // 22
    { +2, +2 },  // 23
};

const std::span<const Delta2D> piece_delta[PIECE_COUNT] = {
    {&piece_delta_data[ 0], &piece_delta_data[ 4]},  // Wazir
    {&piece_delta_data[ 4], &piece_delta_data[12]},  // Knight
    {&piece_delta_data[12], &piece_delta_data[16]},  // Ferz
    {&piece_delta_data[16], &piece_delta_data[20]},  // Dabbaba
    {&piece_delta_data[20], &piece_delta_data[24]},  // Alfil
};

// Note: technically we can skip the wazir, since if the game is not over yet,
// then both sides have 1 wazir, so they cancel out.
static constexpr int piece_values[PIECE_COUNT] = {
    100,  // 1x Wazir    (0.1)
      3,  // 1x Knight   (1.2)
      2,  // 2x Ferz     (1.1)
      2,  // 4x Dabbaba  (0.2)
      1,  // 8x Alfil    (2.2)
};

// Recalculates scores in the state.
//
// Important: this logic must be kept in sync with ExecuteMove() and
// UndoMove() below, wich update the score incrementally.
void RecalculateScores(State &state) {
    int count[2][PIECE_COUNT];
    for (int c = 0; c < COLOR_COUNT; ++c) {
        for (int p = 0; p < PIECE_COUNT; ++p) {
            count[c][p] = state.captured[c][p];
        }
    }
    for (field_t f : state.fields) {
        if (!IsEmpty(f)) count[Color(f)][Piece(f)]++;
    }
    state.scores[0] = 0;
    state.scores[1] = 0;
    for (int p = 0; p < PIECE_COUNT; ++p) {
        int delta = count[0][p] - count[1][p];
        assert(delta % 2 == 0);
        if (delta > 0) state.scores[0] += ( delta / 2)*piece_values[p];
        if (delta < 0) state.scores[1] += (-delta / 2)*piece_values[p];
    }
}

}  // namespace

State State::Create(field_t fields[FIELD_COUNT], uint8_t captured[2][PIECE_COUNT], int turn) {
    State res = {};
    memcpy(res.fields,   fields,   sizeof(res.fields));
    memcpy(res.captured, captured, sizeof(res.captured));
    res.turn = turn;
    RecalculateScores(res);
    return res;
}

struct UndoState ExecuteMove(State &state, const Move &move) {
    piece_t old_piece = PIECE_COUNT;
    if (move.src < FIELD_COUNT) {
        if (!IsEmpty(state.fields[move.dst])) {
            old_piece = Piece(state.fields[move.dst]);
            color_t next_player = state.NextPlayer();
            ++state.captured[next_player][old_piece];
            state.scores[next_player] += piece_values[old_piece];
        }
        state.fields[move.dst] = state.fields[move.src];
        state.fields[move.src] = EMPTY_FIELD;
    } else {
        color_t color = state.NextPlayer();
        piece_t piece = static_cast<piece_t>(move.src - FIELD_COUNT);
        assert(state.captured[color][piece] > 0 && IsEmpty(state.fields[move.dst]));
        state.fields[move.dst] = Field(color, piece);
        --state.captured[color][piece];
    }
    ++state.turn;
    return UndoState{
        .src = move.src,
        .dst = move.dst,
        .old_piece = old_piece,
    };
}

void UndoMove(State &state, const UndoState &undo) {
    --state.turn;
    if (undo.src < FIELD_COUNT) {
        // Undo move src->dst
        state.fields[undo.src] = state.fields[undo.dst];
        if (undo.old_piece < PIECE_COUNT) {
            color_t next_player = state.NextPlayer();
            state.scores[next_player] -= piece_values[undo.old_piece];
            --state.captured[next_player][undo.old_piece];
            state.fields[undo.dst] = Field(Other(next_player), undo.old_piece);
        } else {
            state.fields[undo.dst] = EMPTY_FIELD;
        }
    } else {
        // Undo drop on dst
        ++state.captured[state.NextPlayer()][undo.src - FIELD_COUNT];
        state.fields[undo.dst] = EMPTY_FIELD;
    }
}

void ExecuteSetupMove(State &state, const SetupMove &move) {
    color_t next_player = state.NextPlayer();
    field_t *fp = &state.fields[next_player == RED ? 0 : FIELD_COUNT - move.pieces.size()];
    for (size_t i = 0; i < move.pieces.size(); ++i) {
        fp[i] = Field(next_player, move.pieces[i]);
    }
    ++state.turn;
}

size_t GenerateAllMoves(const State &state, Move (&moves)[MAX_MOVES]) {
    color_t next_player = state.NextPlayer();
    size_t nmove = 0;

    // Move any piece:
    for (int r1 = 0; r1 < 8; ++r1) {
        for (int c1 = 0; c1 < 8; ++c1) {
            field_t field = state.FieldAt(r1, c1);
            if (HasColor(field, next_player)) {
                piece_t piece = Piece(field);
                for (auto [dr, dc] : piece_delta[piece]) {
                    int r2 = r1 + dr;
                    int c2 = c1 + dc;
                    if (InBounds(r2, c2) && !HasColor(state.FieldAt(r2, c2), next_player)) {
                        moves[nmove++] = Move{
                            .src=FieldIndex(r1, c1),
                            .dst=FieldIndex(r2, c2),
                        };
                    }
                }
            }
        }
    }

    // Deploy a previously captured piece:
    for (int p = 0; p < PIECE_COUNT; ++p) {
        if (state.captured[next_player][p] > 0) {
            for (int dst = 0; dst < FIELD_COUNT; ++dst) {
                if (IsEmpty(state.fields[dst])) {
                    Move move = {};
                    move.src = FIELD_COUNT + p;
                    move.dst = dst;
                    moves[nmove++] = move;
                }
            }
        }
    }

    assert(nmove <= MAX_MOVES);
    return nmove;
}

std::vector<Move> GenerateAllMoves(const State &state) {
    Move moves[MAX_MOVES];
    size_t nmove = GenerateAllMoves(state, moves);
    return std::vector<Move>(moves, moves + nmove);
}

std::optional<std::pair<color_t, piece_t>> ParseColoredPiece(char ch) {
    for (int c = 0; c < COLOR_COUNT; ++c) {
        auto p = piece_chars[c].find(ch);
        if (p != std::string_view::npos) {
            return {{static_cast<color_t>(c), static_cast<piece_t>(p)}};
        }
    }
    return {};
}

char FormatColoredPiece(color_t color, piece_t piece) {
    assert(0 <= color && color < COLOR_COUNT);
    assert(0 <= piece && piece < PIECE_COUNT);
    return piece_chars[color][piece];
}

std::optional<Move> ParseMove(color_t color, std::string_view s) {
    if (s.size() == 3) {
        auto p = piece_chars[color].find(s[0]);
        int r = ParseRow(s[1]);
        int c = ParseCol(s[2]);
        if (p == std::string_view::npos || r == -1 || c == -1) return {};
        return Move{
            .src = static_cast<uint8_t>(64 + p),
            .dst = FieldIndex(r, c),
        };
    }

    if (s.size() == 4) {
        int r1 = ParseRow(s[0]);
        int c1 = ParseCol(s[1]);
        int r2 = ParseRow(s[2]);
        int c2 = ParseCol(s[3]);
        if (r1 == -1 || c1 == -1 || r2 == -1 || c2 == -1) return {};
        return Move{
            .src = FieldIndex(r1, c1),
            .dst = FieldIndex(r2, c2),
        };
    }

    return {};
}

std::optional<SetupMove> ParseSetupMove(color_t color, std::string_view s) {
    SetupMove move;
    if (s.size() != move.pieces.size()) return {};
    int counts[PIECE_COUNT] = {};
    for (size_t i = 0; i < move.pieces.size(); ++i) {
        auto p = piece_chars[color].find(s[i]);
        if (p == std::string_view::npos) return {};
        move.pieces[i] = static_cast<piece_t>(p);
        counts[p]++;
    }
    if (!std::ranges::equal(piece_counts, counts)) return {};
    return move;
}

std::string FormatMove(color_t color, const Move &move) {
    if (move.IsNull()) {
        return "NULL";
    } else if (move.src < FIELD_COUNT) {
        std::string s(4, '\0');
        s[0] = FormatRow(Row(move.src));
        s[1] = FormatCol(Col(move.src));
        s[2] = FormatRow(Row(move.dst));
        s[3] = FormatCol(Col(move.dst));
        return s;
    } else {
        assert(move.src - FIELD_COUNT < PIECE_COUNT);
        std::string s(3, '\0');
        s[0] = FormatColoredPiece(color, static_cast<piece_t>(move.src - FIELD_COUNT));
        s[1] = FormatRow(Row(move.dst));
        s[2] = FormatCol(Col(move.dst));
        return s;
    }
}

std::string FormatSetupMove(color_t color, const SetupMove &setup_move) {
    std::string s(setup_move.pieces.size(), '\0');
    for (size_t i = 0; i < setup_move.pieces.size(); ++i) {
        s[i] = FormatColoredPiece(color, setup_move.pieces[i]);
    }
    return s;
}

void DebugPrint(std::ostream &os, const State &state) {
    os << "After " << state.turn << " turns:\n";
    os << "  ";
    for (int c = 0; c < 8; ++c) {
        os << ' ' << FormatCol(c);
    }
    os << '\n';
    for (int r = 0; r < 8; ++r) {
        os << ' ' << FormatRow(r);
        for (int c = 0; c < 8; ++c) {
            os << ' ' ;
            field_t field = state.FieldAt(r, c);
            if (field == EMPTY_FIELD) {
                os << '.';
            } else {
                os << FormatColoredPiece(Color(field), Piece(field));
            }
        }
        os << '\n';
    }
    os << "Captured:";
    for (int c = 0; c < COLOR_COUNT; ++c) {
        os << ' ';
        for (int p = 0; p < PIECE_COUNT; ++p) {
            for (int n = 0; n < state.captured[c][p]; ++n) {
                os << piece_chars[c][p];
            }
        }
    }
    os << '\n';
}
