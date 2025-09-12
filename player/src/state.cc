#include "state.h"

#include <cassert>
#include <span>
#include <string_view>

namespace {

const std::string_view piece_chars[2] = {
  "WNFDA",
  "wnfda",
};

struct Delta2D { int8_t dr, dc; };

const Delta2D piece_delta_data[24] = {
  // Wazir
  { -1,  0 },  //  0
  {  0, -1 },  //  1
  {  0, +1 },  //  2
  { +1,  0 },  //  3
  // Knight
  { -2, -1 },  //  4
  { -2, +1 },  //  5
  { -1, -2 },  //  6
  { -1, +2 },  //  7
  { +1, -2 },  //  8
  { +1, +2 },  //  9
  { +2, -1 },  // 10
  { +2, +1 },  // 11
  // Ferz
  { -1, -1 },  // 12
  { -1, +1 },  // 13
  { +1, -1 },  // 14
  { +1, +1 },  // 15
  // Dabbaba
  { -2,  0 },  // 16
  {  0, -2 },  // 17
  {  0, +2 },  // 18
  { +2,  0 },  // 19
  // Alfil
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

}  // namespace

bool IsGameOver(const State &state) {
  return state.turn >= 2 && (
      std::ranges::find(state.fields, Field(RED,  WAZIR)) == std::ranges::end(state.fields) ||
      std::ranges::find(state.fields, Field(BLUE, WAZIR)) == std::ranges::end(state.fields));
}

void ExecuteMove(State &state, const Move &move) {
  if (move.src < FIELD_COUNT) {
    if (!IsEmpty(state.fields[move.dst])) {
      color_t color = state.NextPlayer();
      piece_t piece = Piece(state.fields[move.dst]);
      ++state.captured[color][piece];
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
}

void ExecuteSetupMove(State &state, const SetupMove &move) {
  constexpr int m = std::size(move.pieces);
  static_assert(m == 16);
  color_t next_player = state.NextPlayer();
  field_t *fp = &state.fields[next_player == RED ? 0 : FIELD_COUNT - m];
  for (size_t i = 0; i < m; ++i) {
    fp[i] = Field(next_player, move.pieces[i]);
  }
  ++state.turn;
}

std::vector<Move> GenerateAllMoves(const State &state) {
  color_t next_player = state.NextPlayer();
  std::vector<Move> moves;

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
            moves.push_back(Move{.src=FieldIndex(r1, c1), .dst=FieldIndex(r2, c2)});
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
          moves.push_back(move);
        }
      }
    }
  }
  return moves;
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
  constexpr int n = std::size(move.pieces);
  static_assert(n == 16);
  if (s.size() != n) return {};
  int counts[PIECE_COUNT] = {};
  for (size_t i = 0; i < n; ++i) {
    auto p = piece_chars[color].find(s[i]);
    if (p == std::string_view::npos) return {};
    move.pieces[i] = static_cast<piece_t>(p);
    counts[p]++;
  }
  if (!std::ranges::equal(piece_counts, counts)) return {};
  return move;
}

std::string FormatMove(color_t color, const Move &move) {
  if (move.src < FIELD_COUNT) {
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
  constexpr int n = std::size(setup_move.pieces);
  std::string s(n, '\0');
  for (size_t i = 0; i < n; ++i) {
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
}
