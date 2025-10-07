#include "transcript.h"

#include "codec.h"
#include "state.h"

#include <algorithm>

std::variant<ParseTranscriptResult, ParseTranscriptError>
ParseTranscript(std::span<const char* const> args) {
    State state = State::Initial();
    if (!args.empty() && !ParseSetupMove(state.NextPlayer(), args[0])) {
        // If the first argument isn't a valid setup move, then we assume
        // it must be a state string.
        std::optional<State> res = DecodeCompactState(args[0]);
        if (!res) {
            return ParseTranscriptError{
                "Could not parse initial argument "
                "(as compact state or setup move)",
                args[0]};
        }
        state = *res;
        args = args.subspan(1);
    }

    std::vector<State> arg_states;
    std::vector<Turn> arg_turns;
    arg_states.push_back(state);
    for (const char *arg : args) {
        Turn turn = {};
        if (state.turn < 2) {
            auto setup_move = ParseSetupMove(state.NextPlayer(), arg);
            if (!setup_move) {
                return ParseTranscriptError{"Could not parse setup move", arg};
            }
            ExecuteSetupMove(state, *setup_move);
            turn = *setup_move;
        } else {
            auto move = ParseMove(state.NextPlayer(), arg);
            if (!move) {
                return ParseTranscriptError{"Could not parse move", arg};
            }
            auto all_moves = GenerateAllMoves(state);
            if (std::ranges::find(all_moves, *move) == all_moves.end()) {
                return ParseTranscriptError{"Illegal move", arg};
            }
            ExecuteMove(state, *move);
            turn = *move;
        }
        arg_turns.push_back(turn);
        arg_states.push_back(state);
    }
    assert(arg_states.size() == arg_turns.size() + 1);
    return ParseTranscriptResult{std::move(arg_states), std::move(arg_turns)};
}

std::string FormatTurn(color_t color, const Turn &t) {
    if (std::holds_alternative<SetupMove>(t)) {
        return FormatSetupMove(color, std::get<SetupMove>(t));
    }
    if (std::holds_alternative<Move>(t)) {
        return FormatMove(color, std::get<Move>(t));
    }
    return "";
}
