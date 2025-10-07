#include "transcript.h"

#include "codec.h"
#include "state.h"

#include <algorithm>

std::vector<State> arg_states;
std::vector<Turn> arg_turns;

bool ParseTranscript(std::span<const char* const> args) {
    if (args.empty()) {
        std::cerr << "Missing arguments.\n";
        return false;
    }

    State state = State::Initial();
    if (!ParseSetupMove(state.NextPlayer(), args[0])) {
        // If the first argument isn't a valid setup move, then we assume
        // it must be a state string.
        std::optional<State> res = DecodeCompactState(args[0]);
        if (!res) {
            std::cerr << "Could not parse initial argument "
                    "(as compact state or setup move): " << args[0] << '\n';
            return false;
        }
        state = *res;
        args = args.subspan(1);
    }

    arg_states.push_back(state);
    for (const char *arg : args) {
        Turn turn = {};
        if (state.turn < 2) {
            auto setup_move = ParseSetupMove(state.NextPlayer(), arg);
            if (!setup_move) {
                std::cerr << "Could not parse setup move: " << arg << '\n';
                return false;
            }
            ExecuteSetupMove(state, *setup_move);
            turn = *setup_move;
        } else {
            auto move = ParseMove(state.NextPlayer(), arg);
            if (!move) {
                std::cerr << "Could not parse move: " << arg << '\n';
                return false;
            }
            auto all_moves = GenerateAllMoves(state);
            if (std::ranges::find(all_moves, *move) == all_moves.end()) {
                std::cerr << "Invalid move: " << arg << '\n';
                return false;
            }
            ExecuteMove(state, *move);
            turn = *move;
        }
        arg_turns.push_back(turn);
        arg_states.push_back(state);
    }
    assert(arg_states.size() == arg_turns.size() + 1);
    return true;
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
