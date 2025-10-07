#ifndef TRANSCRIPT_H_INCLUDED
#define TRANSCRIPT_H_INCLUDED

#include <span>
#include <variant>
#include <vector>

#include "state.h"

using Turn = std::variant<SetupMove, Move>;

using ParseTranscriptResult = std::pair<std::vector<State>, std::vector<Turn>>;
using ParseTranscriptError  = std::pair<const char*, const char*>;

// Parses a transcript from a list of strings (usually command line arguments),
// and returns the result as a pair of two vectors: states and turns.
//
// `states` contains all the game states, and `turns` the turns between states,
// so that states[i] is the state before turn[i], and states[i + 1] the state
// after. It is guaranteed that states.size() == turns.size() + 1.
//
// At most the first two turns can be SetupMoves, the rest are regular Moves.
//
// If an error occurs during parsing, an error message is returned as a pair
// of strings: the first describes the error, and the second is element of
// `args` that caused the error.
std::variant<ParseTranscriptResult, ParseTranscriptError>
ParseTranscript(std::span<const char* const> args);

// Formats a turn as a string.
//
// This is a small wrapper around FormatMove() and FormatSetupMove().
std::string FormatTurn(color_t color, const Turn &t);

#endif  // ndef TRANSCRIPT_H_INCLUDED
