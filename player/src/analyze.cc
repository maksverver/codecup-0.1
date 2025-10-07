// Tool to analyze game positions.
//
// Takes either a list of moves, a state string, or a state string
// followed by a list of moves as arguments. Example:
//
//  analyzer AWADFAANDAFDAADA adaadfadnaafdawa a6c4 h1f2 b2d4
//  analyzer CjZbrWtZGYBJRALAgZo__h5u___fD g5e3 d4e3 h2g3 Af3
//

#include <cassert>
#include <span>
#include <string_view>
#include <vector>

#include "analysis.h"
#include "options.h"
#include "state.h"
#include "transcript.h"

DECLARE_OPTION(bool, arg_help, false, "help", "show usage information");

DECLARE_OPTION(int, arg_print_moves, 5, "print-moves", "number of best moves to print");

int main(int argc, char *argv[]) {
    std::vector<char*> plain_args;
    if ( !ParseOptions(argc, argv, plain_args) ||
         arg_help ||
         !ParseTranscript(plain_args)) {
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

            if (PnsResult pns = FindWinningMove(state); pns.status == 1) {
                std::cout << "PNS: won! Nodes expanded: " << pns.nodes_expanded << "; "
                    << "winning move: " << FormatMove(state.NextPlayer(), pns.winning_move) << '\n';
            } else if (pns.status == -1) {
                std::cout << "PNS: lost! Nodes expanded: " << pns.nodes_expanded << "\n";
            } else {
                assert(pns.status == 0);
                std::cout << "PNS: incomplete (nodes expanded: " << pns.nodes_expanded << ")\n";
            }
        }
        if (i < arg_turns.size()) {
            std::cout
                << "Turn " << i << ": "
                << FormatTurn(state.NextPlayer(), arg_turns[i]) << '\n';
        }
    }
}
