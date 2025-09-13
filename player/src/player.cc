#include "logging.h"
#include "options.h"
#include "random.h"
#include "state.h"

#include <cassert>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#ifndef LOCAL_BUILD
#define LOCAL_BUILD 0
#endif

namespace {

// TODO: real player name
const std::string player_name = "Olive";

DECLARE_OPTION(bool, arg_help, false, "help",
    "show usage information");

DECLARE_OPTION(std::string, arg_seed, "", "seed",
    "Random seed in hexadecimal format. If empty, pick randomly. "
    "The chosen seed will be logged to stderr for reproducibility.");

DECLARE_OPTION(int, arg_depth, 2, "depth", "Maximum search depth.");

/*
DECLARE_OPTION(int, arg_time_limit, LOCAL_BUILD ? 0 : 25, "time-limit",
    "Time limit in seconds (or 0 to disable time-based performance). "
    "On each turn, the player uses a fraction of time remaining on analysis. "
    "Note that this should be slightly lower than the official time limit to "
    "account for overhead.");
*/

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

constexpr int val_inf = 999999999;
constexpr int val_win = 900000000;

// Evaluates an intermediate game state.
//
// Precondition: state.GameOver() == false
int Evaluate(const State &state) {
  // Note: technically we can skip the wazir, since if the game is not over yet,
  // then both sides have 1 wazir, so they cancel out.
  static constexpr int piece_values[PIECE_COUNT] = {
    100,  // 1x Wazir    (0.1)
      3,  // 1x Knight   (1.2)
      2,  // 2x Ferz     (1.1)
      2,  // 4x Dabbaba  (0.2)
      1,  // 8x Alfil    (2.2)
  };

  int value = 0;
  for (int piece = 0; piece < PIECE_COUNT; ++piece) {
    value += (state.captured[RED][piece] - state.captured[BLUE][piece]) * piece_values[piece];
  }
  return state.NextPlayer() == RED ? value : -value;
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

  int best_value = -val_inf;
  for (const Move &move : GenerateAllMoves(state)) {
    UndoState undo = ExecuteMove(state, move);
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

void PlayGame(rng_t &rng) {
  Timer timer(false);

  State state = {};

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
    // Maybe TODO: print compact state
    //DebugPrint(std::cerr, state);

    if (state.NextPlayer() == my_color) {
      auto pause_duration = timer.Resume();
      LogPause(pause_duration, timer.Elapsed(false));
      // Calculate my move.
      std::string output;
      if (state.turn < 2) {
        // Place initial pieces.
        SetupMove setup_move = {
          .pieces = {
            WAZIR, KNIGHT, FERZ, FERZ, DABBABA, DABBABA, DABBABA, DABBABA,
            ALFIL, ALFIL, ALFIL, ALFIL, ALFIL, ALFIL, ALFIL, ALFIL },
        };
        // Generate a random permutation. TODO: do something smarter here.
        std::ranges::shuffle(setup_move.pieces, rng);
        output = FormatSetupMove(state.NextPlayer(), setup_move);
        ExecuteSetupMove(state, setup_move);
      } else {
        std::vector<Move> all_moves = GenerateAllMoves(state);
        assert(!all_moves.empty());
        auto [best_moves, best_score] = FindBestMoves(state, all_moves);
        LogMoveCount(all_moves.size(), best_moves.size(), best_score);
        assert(!best_moves.empty());
        Move move = RandomSample(best_moves, rng);
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
          LogError() << "Opponent's move is invalid: " << line;
          exit(1);
        }
        ExecuteMove(state, *move);
      }
    }
  }
  LogInfo() << "Game over.";
}

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
  LogId('R', player_name);

  if (!ParseOptions(argc, argv) || arg_help) {
    std::ostream &os = arg_help ? std::cout : std::clog;
    os << "\nOptions:\n";
    PrintOptionUsage(os);
    return EXIT_FAILURE;
  }

  // Initialize RNG.
  rng_seed_t seed;
  if (!InitializeSeed(seed, arg_seed)) return EXIT_FAILURE;
  LogSeed(seed);
  rng_t rng = CreateRng(seed);

  PlayGame(rng);
}
