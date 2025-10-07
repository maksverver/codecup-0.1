#ifndef TRANSCRIPT_H_INCLUDED
#define TRANSCRIPT_H_INCLUDED

#include <span>
#include <variant>
#include <vector>

#include "state.h"

using Turn = std::variant<SetupMove, Move>;

// List of states.
//
// If not empty, arg_states.size() == arg_turns.size() + 1, and arg_states[i]
// contains the game state before turn[i], and arg_states[i + 1] the state after.
extern std::vector<State> arg_states;

// List of turns. At most the first two turns can be SetupMoves, the following
// turns are regular mvoes.
extern std::vector<Turn> arg_turns;

// Parses a transcript from command line arguments, writes the results to
// arg_states and arg_turns, and returns true.
//
// When an error occurs, a message is printed to stderr, and this function
// returns false.
//
// This function is intended to be called from main() to parse command line
// arguments, and if it fails, the program should exit immediately.
bool ParseTranscript(std::span<const char* const> args);

// Formats a turn as a string.
//
// This is a small wrapper around FormatMove() and SetupMove().
std::string FormatTurn(color_t color, const Turn &t);

#endif  // ndef TRANSCRIPT_H_INCLUDED
