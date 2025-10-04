#ifndef ANALYSIS_H_INCLUDED
#define ANALYSIS_H_INCLUDED

#include "state.h"

constexpr int val_inf = 999999999;
constexpr int val_win = 900000000;

// Evaluates an intermediate game state.
//
// Precondition: state.GameOver() == false
int Evaluate(const State &state);

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
int Search(State &state, int depth_left, int alpha, int beta);

// Returns a list of best moves paired with the maximum game tree value.
std::pair<std::vector<Move>, int> FindBestMoves(State state, const std::vector<Move> &all_moves);

struct PnsResult {
  // +1 if proven win, -1 if proven loss, 0 if the search was aborted.
  int status = 0;

  // Winning move (if status == +1)
  Move winning_move = Move::Null();

  // Informative; number of nodes expanded during the search.
  int nodes_expanded = 0;
};

// Searches for a winning move using Proof Number Search.
PnsResult FindWinningMove(const State &state);

#endif // ndef ANALYSIS_H_INCLUDED
