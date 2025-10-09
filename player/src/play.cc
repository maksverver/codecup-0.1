#include "analysis.h"
#include "logging.h"
#include "options.h"
#include "random.h"
#include "state.h"
#include "weights.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iterator>
#include <iostream>
#include <optional>
#include <ranges>
#include <string>
#include <string_view>
#include <utility>

#ifndef LOCAL_BUILD
#define LOCAL_BUILD 0
#endif

#if LOCAL_BUILD
#include "transcript.h"
#endif

namespace {

// TODO: real player name
const std::string player_name = "Olive";

DECLARE_OPTION(bool, arg_help, false, "help",
        "show usage information");

DECLARE_OPTION(std::string, arg_seed, "", "seed",
        "Random seed in hexadecimal format. If empty, pick randomly. "
        "The chosen seed will be logged to stderr for reproducibility.");

DECLARE_OPTION(int, arg_random_setup, 1, "random-setup",
        "How to randomize starting pieces: 0=fixed layout "
        "1=shuffle while avoiding pieces covering the same subset of squares "
        "2=shuffle all");

/*
DECLARE_OPTION(int, arg_time_limit, LOCAL_BUILD ? 0 : 25, "time-limit",
        "Time limit in seconds (or 0 to disable time-based performance). "
        "On each turn, the player uses a fraction of time remaining on analysis. "
        "Note that this should be slightly lower than the official time limit to "
        "account for overhead.");
*/

#if LOCAL_BUILD
DECLARE_OPTION(bool, arg_move, false, "move", "Output a single move. "
        "Game state is passed via command line arguments.");
#endif

// A simple timer. Can be running or paused. Tracks time both while running and
// while paused. Use Elapsed() to query, Pause() and Resume() to switch states.
class Timer {
public:
    Timer(bool running = true) : running(running) {}

    bool Running() const { return running; }
    bool Paused() const { return !running; }

    // Returns how much time passed in the given state, in total.
    log_duration_t Elapsed(bool while_running = true) const {
        clock_t::duration d = elapsed[while_running];
        if (running == while_running) d += clock_t::now() - start;
        return std::chrono::duration_cast<log_duration_t>(d);
    }

    log_duration_t Pause() {
        assert(Running());
        return TogglePause();
    }

    log_duration_t Resume() {
        assert(Paused());
        return TogglePause();
    }

    // Toggles running state, and returns how much time passed since last toggle.
    log_duration_t TogglePause() {
        auto end = clock_t::now();
        auto delta = end - start;
        elapsed[running] += delta;
        start = end;
        running = !running;
        return std::chrono::duration_cast<log_duration_t>(delta);
    }

private:
    using clock_t = std::chrono::steady_clock;

    bool running = false;
    clock_t::time_point start = clock_t::now();
    clock_t::duration elapsed[2] = {clock_t::duration{0}, clock_t::duration{0}};
};

std::string ReadInputLine() {
    std::string s;
    if (!std::getline(std::cin, s) || s.empty()) {
        LogError() << "Unexpected end of input!";
        exit(1);
    }
    LogReceived(s);
    if (s == "Quit") {
        LogInfo() << "Exiting.";
        exit(0);
    }
    return s;
}

constexpr int SETUP_COUNT = 1 << 15;

// Given an index between 0 and SETUP_COUNT (exclusive), generates the pieces
// to set up, so that pieces of the same kind cover different subsets of the
// squares on the board. See docs/setup.txt and tools/enum-setup.py for details.
std::array<piece_t, 16> GenerateSetup(unsigned index) {
    assert(index < SETUP_COUNT);
    auto next_bit = [&index]() {
        int res = index & 1;
        index >>= 1;
        return res;
    };
    std::array<piece_t, 16> pieces;
    std::ranges::fill(pieces, WAZIR);
    auto get_field = [&](const std::ranges::forward_range auto &opts) -> piece_t& {
        auto values = opts | std::views::filter([&](int i) { return pieces[i] == WAZIR; });
        assert(std::ranges::distance(values) == 2);
        return pieces[*std::next(values.begin(), next_bit())];
    };
    get_field(std::array<int,  2>{  0,  4 }) = ALFIL;
    get_field(std::array<int,  2>{  1,  5 }) = ALFIL;
    get_field(std::array<int,  2>{  2,  6 }) = ALFIL;
    get_field(std::array<int,  2>{  3,  7 }) = ALFIL;
    get_field(std::array<int,  2>{  8, 12 }) = ALFIL;
    get_field(std::array<int,  2>{  9, 13 }) = ALFIL;
    get_field(std::array<int,  2>{ 10, 14 }) = ALFIL;
    get_field(std::array<int,  2>{ 11, 15 }) = ALFIL;
    get_field(std::array<int,  4>{  0,  2,  4,  6 }) = DABBABA;
    get_field(std::array<int,  4>{  1,  3,  5,  7 }) = DABBABA;
    get_field(std::array<int,  4>{  8, 10, 12, 14 }) = DABBABA;
    get_field(std::array<int,  4>{  9, 11, 13, 15 }) = DABBABA;
    get_field(std::array<int,  8>{  0,  2,  4,  6,  9, 11, 13, 15 }) = FERZ;
    get_field(std::array<int,  8>{  1,  3,  5,  7,  8, 10, 12, 14 }) = FERZ;
    get_field(std::array<int, 16>{  0,  1,  2,  3,  4,  5,  6,  7,
                                    8,  9, 10, 11, 12, 13, 14, 15 }) = KNIGHT;
    return pieces;
}

SetupMove GenerateSetupMove(const State &state, rng_t &rng) {
    std::array<piece_t, 16> pieces = {
            ALFIL,   ALFIL,   DABBABA, WAZIR,   KNIGHT,  DABBABA, ALFIL,   ALFIL,
            FERZ,    DABBABA, ALFIL,   ALFIL,   ALFIL,   ALFIL,   DABBABA, FERZ,
    };
    if (arg_random_setup == 0) {
        if (state.NextPlayer() == BLUE) {
            std::ranges::reverse(pieces);
        }
    } else if (arg_random_setup == 1) {
        std::ranges::shuffle(pieces, rng);
    } else {
        assert(arg_random_setup == 2);
        std::uniform_int_distribution<unsigned> dist(0, SETUP_COUNT - 1);
        pieces = GenerateSetup(dist(rng));
    }
    return SetupMove{.pieces = pieces};
}

void PlayGame(rng_t &rng) {
    Timer timer(false);

    State state = State::Initial();

    // First line of input contains either "Start" if I play first, or else the
    // first move played by the opponent.
    color_t my_color;
    std::optional<std::string> next_line;
    if (std::string line = ReadInputLine(); line == "Start") {
        my_color = RED;
    } else {
        my_color = BLUE;
        next_line = std::move(line);
    }
    while (!state.GameOver()) {
        if (state.NextPlayer() == my_color) {
            auto pause_duration = timer.Resume();
            LogPause(pause_duration, timer.Elapsed(false));
            // Calculate my move.
            std::string output;
            if (state.turn < 2) {
                // Place initial pieces.
                SetupMove setup_move = GenerateSetupMove(state, rng);
                output = FormatSetupMove(state.NextPlayer(), setup_move);
                ExecuteSetupMove(state, setup_move);
            } else {
                Move move = Move::Null();
                PnsResult pns_res = FindWinningMove(state);
                LogPnsResult(pns_res.status, pns_res.nodes_expanded);
                if (pns_res.status == 1) {
                    move = pns_res.winning_move;
                } else {
                    std::vector<Move> all_moves = GenerateAllMoves(state);
                    assert(!all_moves.empty());
                    auto [best_moves, best_score] = FindBestMoves(state, all_moves);
                    LogMoveCount(all_moves.size(), best_moves.size(), best_score);
                    assert(!best_moves.empty());
                    move = RandomSample(best_moves, rng);
                }
                output = FormatMove(state.NextPlayer(), move);
                ExecuteMove(state, move);
            }
            LogSending(output);
            // Pause the timer just before writing the output line, since the referee
            // may suspend our process immediately after.
            auto turn_duration = timer.Pause();
            LogTime(turn_duration, timer.Elapsed(true));
            std::cout << output << std::endl;
        } else {
            // Opponent's turn.
            std::string line = next_line ? *std::exchange(next_line, std::nullopt) : ReadInputLine();
            if (state.turn < 2) {
                std::optional<SetupMove> setup_move = ParseSetupMove(state.NextPlayer(), line);
                if (!setup_move) {
                    LogError() << "Could not parse setup move: " << line;
                    exit(1);
                }
                // TODO: validate setup move?
                ExecuteSetupMove(state, *setup_move);
            } else {
                std::vector<Move> all_moves = GenerateAllMoves(state);
                std::optional<Move> move = ParseMove(state.NextPlayer(), line);
                if (!move) {
                    LogError() << "Could not parse opponent's move: " << line;
                    exit(1);
                }
                if (std::ranges::find(all_moves, *move) == all_moves.end()) {
                    LogError() << "Opponent's move is illegal: " << line;
                    exit(1);
                }
                ExecuteMove(state, *move);
            }
        }
    }
    LogInfo() << "Game over.";
}

#if LOCAL_BUILD
void PlaySingleMove(const State &state, rng_t &rng) {
    if (state.GameOver()) {
        std::cerr << "Game is over!";
        exit(1);
    }
    color_t player = state.NextPlayer();
    std::string output;
    if (state.turn < 2) {
        SetupMove move = GenerateSetupMove(state, rng);
        output = FormatSetupMove(player, move);
    } else {
        PnsResult pns_res = FindWinningMove(state);
        Move move = Move::Null();
        if (pns_res.status == 1) {
            move = pns_res.winning_move;
        } else {
            std::vector<Move> all_moves = GenerateAllMoves(state);
            if (all_moves.empty()) {
                std::cerr << "No moves left!";
                exit(1);
            }
            auto [best_moves, best_score] = FindBestMoves(state, all_moves);
            assert(!best_moves.empty());
            move = RandomSample(best_moves, rng);
        }
        output = FormatMove(player, move);
    }
    assert(!output.empty());
    std::cout << output << std::endl;
}
#endif

bool InitializeSeed(rng_seed_t &seed, std::string_view hex_string) {
    if (hex_string.empty()) {
        // Generate a new random 128-bit seed
        seed = GenerateSeed(4);
        return true;
    }
    if (auto s = ParseSeed(hex_string)) {
        seed = *s;
        return true;
    } else {
        LogError() << "Could not parse RNG seed: [" << hex_string << "]";
        return false;
    }
}

} // namespace

int main(int argc, char *argv[]) {
    // Debug-print semi-random setup moves.
    if (false) {
        for (int i = 0; i < SETUP_COUNT; ++i) {
            std::cout << FormatSetupMove(RED, SetupMove{.pieces=GenerateSetup(i)}) << '\n';
        }
        return 0;
    }

    std::vector<char*> plain_args;
    if (!ParseOptions(argc, argv, plain_args) || arg_help) {
        std::ostream &os = arg_help ? std::cout : std::clog;
        LogId('R', player_name);
        os << "\nOptions:\n";
        PrintOptionUsage(os);
        return EXIT_FAILURE;
    }

    InitializeWeights();

    // Initialize RNG.
    rng_seed_t seed;
    if (!InitializeSeed(seed, arg_seed)) return EXIT_FAILURE;
    rng_t rng = CreateRng(seed);

#if LOCAL_BUILD
    if (arg_move) {
        auto parse_res = ParseTranscript(plain_args);
        if (std::holds_alternative<ParseTranscriptError>(parse_res)) {
            auto [msg, arg] = std::get<ParseTranscriptError>(parse_res);
            std::cerr << msg << ": " << arg << std::endl;
            return EXIT_FAILURE;
        }
        assert(std::holds_alternative<ParseTranscriptResult>(parse_res));
        auto [states, turns] = std::get<ParseTranscriptResult>(parse_res);
        assert(!states.empty());
        PlaySingleMove(states.back(), rng);
        return EXIT_SUCCESS;
    }
#endif
    if (!plain_args.empty()) {
        std::clog << "Unexpected command line arguments!\n";
        return EXIT_FAILURE;
    }

    // Play a full game via the Caia protocol.
    LogId('R', player_name);
    LogSeed(seed);
    PlayGame(rng);
}
