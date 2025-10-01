// Tool to analyze game positions.
//
// Takes either a list of moves, a state string, or a state string
// followed by a list of moves as arguments. Example:
//
//  analyzer AWADFAANDAFDAADA adaadfadnaafdawa a6c4 h1f2 b2d4
//  analyzer CjZbrWtZGYBJRALAgZo__h5u___fD g5e3 d4e3 h2g3 Af3
//

#include <algorithm>
#include <cassert>
#include <span>
#include <vector>
#include <variant>

#include "analysis.h"
#include "options.h"
#include "state.h"

DECLARE_OPTION(bool, arg_help, false, "help", "show usage information");

DECLARE_OPTION(int, arg_print_moves, 5, "print-moves", "number of best moves to print");

namespace {

using Turn = std::variant<SetupMove, Move>;

std::vector<State> arg_states;
std::vector<Turn> arg_turns;

bool ParsePlainArgs(std::span<const char* const> args) {
    if (args.empty()) {
        std::cerr << "Missing arguments.\n";
        return false;
    }

    State state = State::Initial();
    if (!ParseSetupMove(state.NextPlayer(), args[0])) {
        // If the first argument isn't a valid setup move, then we assume
        // it must be a state string.
        std::cerr << "TODO: parse state string!\n";
        args = args.subspan(1);
        return false;  // not yet implemented
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

}  // namespace

int main(int argc, char *argv[]) {
    std::vector<char*> plain_args;
    if ( !ParseOptions(argc, argv, plain_args) ||
         arg_help ||
         !ParsePlainArgs(plain_args)) {
        std::ostream &os = arg_help ? std::cout : std::clog;
        os << "\nOptions:\n";
        PrintOptionUsage(os);
        os << "\nArguments: optional state string followed by moves\n";
        return EXIT_FAILURE;
    }

    if (argc < 2) {
        std::cerr << "Missing arguments! "
            "Need either a state string, a sequence of moves, "
            "or a state string followed by moves." << std::endl;
        return EXIT_FAILURE;
    }

    for (size_t i = 0; i < arg_states.size(); ++i) {
        const State &state = arg_states[i];
        if (state.turn >= 2) {
            auto [moves, score] = FindBestMoves(state, GenerateAllMoves(state));
            color_t color = state.NextPlayer();
            std::cout
                << "Player " << int{color} << "; "
                << "Score: " << score << "; "
                << "Best moves:";
            for (size_t j = 0; j < moves.size() && j < (size_t) arg_print_moves; ++j) {
                std::cout << ' ' << FormatMove(state.NextPlayer(), moves[j]);
            }
            if (moves.size() > (size_t) arg_print_moves) {
                std::cout << "... (" << moves.size() << " total)";
            }
            std::cout << std::endl;
        }
        if (i < arg_turns.size()) {
            std::cout
                << "Turn " << i << ": "
                << FormatTurn(state.NextPlayer(), arg_turns[i]) << '\n';
        }
    }
}
