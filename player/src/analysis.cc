#include "analysis.h"

#include <cstring>
#include <vector>
#include <utility>

#include "options.h"

DECLARE_OPTION(int, arg_depth, 4, "depth", "Maximum search depth.");

DECLARE_OPTION(int, arg_tt_depth, 99, "tt-depth", "Minimum search depth left to use transposition table. (0 disables)");

namespace {

// Equality for the purpose of the transposition table.
// Must be consistent with StateHash defined below.
struct StateEqual {
  bool operator()(const State &s, const State &t) const {
    return
      (s.turn & 1) == (t.turn & 1) &&
      memcmp(s.fields,   t.fields,   sizeof(s.fields))   == 0 &&
      memcmp(s.captured, t.captured, sizeof(s.captured)) == 0;
  }
};

// Hash for the purpose of the transposition table.
// Must be consistent with StateEqual defined above.
struct StateHash {
  size_t operator()(const State& state) const {
    // 64-bit FNV-1a; can be made a lot faster with e.g. xxHash, or
    // Zobrist hashing.
    uint64_t hash = 0xcbf29ce484222325;
    auto add = [&hash](uint64_t val) {
      hash ^= val;
      hash *= 0x00000100000001b3;
    };
    for (auto f : state.fields) add(f);
    for (const auto &c : state.captured) for (auto n : c) add(n);
    add(state.turn & 1);
    return hash;
  }
};

struct TTEntry {
  int depth_left  = 0;
  int lower_bound = 0;
  int upper_bound = 0;
};

std::unordered_map<State, TTEntry, StateHash, StateEqual> tt;

static long long nodes_evaluated = 0;
static long long tt_hits = 0;
static long long tt_used = 0;


}  // namespace

// Evaluates an intermediate game state.
//
// Precondition: state.GameOver() == false
int Evaluate(const State &state) {
  int value = state.scores[0] - state.scores[1];
  return state.NextPlayer() == 0 ? value : -value;
}

// Minimax search with alpha-beta pruning
//
// Uses depth-first search to determine the value of the game tree expanded to
// the given depth. Returns a value v such that if:
//
//  alpha < v < beta: v is the exact game tree value
//  v <= alpha:       v is an upper bound on the exact value
//  beta <= v:        v is a lower bound on the exact value
//
// Precondition: alpha < beta
int Search(State &state, int depth_left, int alpha, int beta) {

  if (state.GameOver()) {
    int value = val_win + depth_left;
    return state.Winner() == state.NextPlayer() ? value : -value;
  }

  if (depth_left == 0) {
    ++nodes_evaluated;
    return Evaluate(state);
  }

  // Query transposition table.
  TTEntry *entry = nullptr;
  if (depth_left >= arg_tt_depth) {
    entry = &tt[state];
    if (entry->depth_left > 0) ++tt_hits;
    // Note: we do use cached values with *higher* depth_left, which potentially
    // changes the result compared to not using the TT at all.
    // (May want to make this optional for deterministic testing.)
    if (entry->depth_left >= arg_tt_depth) {
      if (entry->lower_bound == entry->upper_bound ||
          entry->lower_bound >= beta) {
        ++tt_used;
        return entry->lower_bound;
      } else if (entry->upper_bound <= alpha) {
        ++tt_used;
        return entry->upper_bound;
      }
    }
  }

  // Try all moves.
  int best_value = -val_inf;
  {
    Move moves[MAX_MOVES];
    size_t nmove = GenerateAllMoves(state, moves);
    int alpha2 = alpha;
    for (size_t i = 0; i < nmove; ++i) {
      UndoState undo = ExecuteMove(state, moves[i]);
      int value = -Search(state, depth_left - 1, -beta, -alpha2);
      UndoMove(state, undo);
      if (value > best_value) {
        best_value = value;
        if (value > alpha2) alpha2 = value;
        if (value >= beta) break;  // beta cut-off
      }
    }
  }

  // Update transposition table.
  if (entry != nullptr && entry->depth_left <= depth_left) {
    entry->lower_bound = best_value > alpha ? best_value : -val_inf;
    entry->upper_bound = best_value < beta  ? best_value : +val_inf;
    entry->depth_left = depth_left;
  }

  return best_value;
}

// Returns a list of best moves paired with the maximum game tree value.
std::pair<std::vector<Move>, int> FindBestMoves(State state, const std::vector<Move> &all_moves) {
  assert(arg_depth > 0);
  std::vector<Move> best_moves;
  int best_value = -val_inf;
  int alpha = -val_inf;
  for (const Move &move : all_moves) {
    UndoState undo = ExecuteMove(state, move);
    int value = -Search(state, arg_depth - 1, -val_inf, -alpha);
    UndoMove(state, undo);
    if (value > best_value) {
      best_moves.clear();
      best_value = value;
      // The -1 here is important because we want to collect ALL best moves.
      // Setting alpha = best_value might give a small speedup, but then only
      // the first move discovered can be used, since for any subsequent moves
      // with value == best_move would only be an upper bound and might not be
      // optimal.
      alpha = value - 1;
    }
    if (value == best_value) {
      best_moves.push_back(move);
    }
  }

  std::cerr
    << "nodes_evaluated=" << nodes_evaluated << ' '
    << "tt_hits=" << tt_hits << ' '
    << "tt_used=" << tt_used << std::endl;

  return {best_moves, best_value};
}
