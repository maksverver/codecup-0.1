#include "analysis.h"

#include <vector>
#include <utility>

#include "options.h"

DECLARE_OPTION(int, arg_depth, 4, "depth", "Maximum search depth.");

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
    return Evaluate(state);
  }

  Move moves[MAX_MOVES];
  size_t nmove = GenerateAllMoves(state, moves);
  int best_value = -val_inf;
  for (size_t i = 0; i < nmove; ++i) {
    UndoState undo = ExecuteMove(state, moves[i]);
    int value = -Search(state, depth_left - 1, -beta, -alpha);
    UndoMove(state, undo);
    if (value > best_value) {
      if (value >= beta) return value;  // beta cut-off
      if (value > alpha) alpha = value;
      best_value = value;
    }
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
  return {best_moves, best_value};
}
