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
#include "weights.h"

DECLARE_OPTION(bool, arg_help, false, "help", "show usage information");

DECLARE_OPTION(int, arg_print_moves, 5, "print-moves", "number of best moves to print");

static void PrintUsage(std::ostream &os) {
    os << "\nOptions:\n";
    PrintOptionUsage(os);
    os << "\nArguments: optional state string followed by moves\n";
}

int main(int argc, char *argv[]) {
    std::vector<char*> plain_args;
    if (!ParseOptions(argc, argv, plain_args) || arg_help) {
        PrintUsage(arg_help ? std::cout : std::clog);
        return EXIT_FAILURE;
    }
    InitializeWeights();
    if (plain_args.empty()) {
        std::cerr << "Missing arguments.\n";
        PrintUsage(std::cerr);
        return EXIT_FAILURE;
    }
    auto parse_res = ParseTranscript(plain_args);
    if (std::holds_alternative<ParseTranscriptError>(parse_res)) {
        auto [msg, arg] = std::get<ParseTranscriptError>(parse_res);
        std::cerr << msg << ": " << arg << std::endl;
        return EXIT_FAILURE;
    }
    assert(std::holds_alternative<ParseTranscriptResult>(parse_res));
    auto [states, turns] = std::get<ParseTranscriptResult>(parse_res);

    if (argc < 2) {
        std::cerr << "Missing arguments! "
            "Need either a state string, a sequence of moves, "
            "or a state string followed by moves." << std::endl;
        return EXIT_FAILURE;
    }

    std::optional<ProofNumberSearch> pns_per_player[2];
    for (size_t i = 0; i < states.size(); ++i) {
        const State &state = states[i];
        std::optional<ProofNumberSearch> &pns = pns_per_player[state.NextPlayer()];
        if (arg_pns_max_nodes > 0) {
            if (state.turn < 2 || state.GameOver()) {
                pns.reset();
            } else if (!pns) {
                pns = ProofNumberSearch::Create(state);
            } else {
                pns->AdvanceState(state);
            }
        }
        if (state.turn >= 2) {
            auto res = SearchMinimax(state, GenerateAllMoves(state));
            color_t color = state.NextPlayer();
            std::cout
                << "Player " << int{color} << "; "
                << "Score: " << res.best_value << "; "
                << "Depth: " << res.search_depth << "; "
                << "Evals: " << res.nodes_evaluated << "; "
                << "TT used/hits: " << res.tt_used + '/' + res.tt_hits << "; "
                << "Best moves:";
            for (size_t j = 0; j < res.best_moves.size() && j < (size_t) arg_print_moves; ++j) {
                std::cout << ' ' << FormatMove(state.NextPlayer(), res.best_moves[j]);
            }
            if (res.best_moves.size() > (size_t) arg_print_moves) {
                std::cout << "... (" << res.best_moves.size() << " total)";
            }
            std::cout << std::endl;

            if (pns) {
                PnsResult res = pns->FindWinningMoves();
                if (res.status == 1) {
                    std::cout << "PNS: won! Nodes expanded: " << res.nodes_expanded << "; "
                        << "winning move: " << FormatMove(state.NextPlayer(), res.winning_move) << '\n';
                } else if (res.status == -1) {
                    std::cout << "PNS: lost! Nodes expanded: " << res.nodes_expanded << "\n";
                } else {
                    assert(res.status == 0);
                    std::cout << "PNS: incomplete (nodes expanded: " << res.nodes_expanded << ")\n";
                }
            }
        }
        if (i < turns.size()) {
            std::cout
                << "Turn " << i << ": "
                << FormatTurn(state.NextPlayer(), turns[i]) << '\n';
        }
    }
}
