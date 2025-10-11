#include "analysis.h"

#include "pns.h"
#include "options.h"
#include "weights.h"

#include <cstring>
#include <vector>
#include <utility>

DECLARE_OPTION(int, arg_max_depth, 4, "max-depth", "Maximum search depth.");

DECLARE_OPTION(int64_t, arg_max_evals, 1000000, "max-evals", "Maximum search states evaluated.");

DECLARE_OPTION(int, arg_tt_depth, 99, "tt-depth", "Minimum search depth left to use transposition table. (0 disables)");

DECLARE_OPTION(bool, arg_pns_init_moves, true, "pns-init-moves", "Initialize PNS nodes using move count");

DECLARE_OPTION(int, arg_pns_max_nodes, 500000, "pns-max-nodes", "Maximum number of PNS nodes (0 disables PNS)");

DECLARE_OPTION(bool, arg_complex_eval, true, "complex-eval", "More detailed evaluation");

DECLARE_OPTION(int, arg_search_ext, 0, "search-ext", "Max. search extensions");

namespace {

// Equality for the purpose of the transposition table.
// Must be consistent with StateHash defined below.
struct StateEqual {
    bool operator()(const State &s, const State &t) const {
        return
            (s.turn & 1) == (t.turn & 1) &&
            memcmp(s.fields,   t.fields,   sizeof(s.fields))   == 0 &&
            memcmp(s.captured, t.captured, sizeof(s.captured)) == 0;
    }
};

// Hash for the purpose of the transposition table.
// Must be consistent with StateEqual defined above.
struct StateHash {
    size_t operator()(const State& state) const {
        // 64-bit FNV-1a; can be made a lot faster with e.g. xxHash, or
        // Zobrist hashing.
        uint64_t hash = 0xcbf29ce484222325;
        auto add = [&hash](uint64_t val) {
            hash ^= val;
            hash *= 0x00000100000001b3;
        };
        for (auto f : state.fields) add(f);
        for (const auto &c : state.captured) for (auto n : c) add(n);
        add(state.turn & 1);
        return hash;
    }
};

struct TTEntry {
    int depth_left  = 0;
    int lower_bound = 0;
    int upper_bound = 0;
};

std::unordered_map<State, TTEntry, StateHash, StateEqual> tt;

struct SearchContext {
    int64_t evals_left = 0;
    int64_t tt_hits = 0;
    int64_t tt_used = 0;
};

}  // namespace

int SimpleEval(const State &state) {
    int value = state.scores[0] - state.scores[1];
    return state.NextPlayer() == 0 ? value : -value;
}

// Complex evaluation of gameplay state.
int ComplexEval(const State &state) {
    constexpr int material_weight      =  8;
    constexpr int drop_move_value      =  1;
    constexpr int wazir_move_bonus     =  5;  // value of the wazir having a move
    constexpr int move_value           =  5;  // move to empty space
    constexpr int guard_value          =  1;  // "attack" space occupied by friendly piece
    constexpr int attack_value         =  8;  // attack space occupied by opponent's piece
    constexpr int wazir_flank_value    =  5;  // attack space next to oponnent's wazir
    constexpr int wazir_attack_value   = 25;  // attack space occupied by opponent's wazir

    int scores[COLOR_COUNT] = {};
    int empty = 0;

    // maybe: add unique fields attacked?

    for (int i = 0; i < FIELD_COUNT; ++i) {
        field_t f = state.fields[i];
        if (IsEmpty(f)) {
            ++empty;
            continue;
        }
        color_t color = Color(f);
        piece_t piece = Piece(f);

        scores[color] += piece_values[piece] * material_weight;

        int r1 = static_cast<unsigned>(i) / 8;
        int c1 = static_cast<unsigned>(i) % 8;
        for (auto [dr, dc] : piece_delta[piece]) {
            int r2 = r1 + dr;
            int c2 = c1 + dc;
            if (!InBounds(r2, c2)) continue;
            field_t g = state.FieldAt(r2, c2);
            if (IsEmpty(g)) {
                if ( (r2 > 0 && state.FieldAt(r2 - 1, c2) == Field(Other(color), WAZIR)) ||
                     (r2 < 7 && state.FieldAt(r2 + 1, c2) == Field(Other(color), WAZIR)) ||
                     (c2 > 0 && state.FieldAt(r2, c2 - 1) == Field(Other(color), WAZIR)) ||
                     (c2 < 7 && state.FieldAt(r2, c2 + 1) == Field(Other(color), WAZIR)) ) {
                    scores[color] += wazir_flank_value;
                } else {
                    scores[color] += move_value;
                }
            } else if (Color(g) == color) {
                scores[color] += guard_value;
            } else if (Piece(g) == WAZIR) {
                scores[color] += wazir_attack_value;
            } else {
                scores[color] += attack_value;
            }
            if (piece == WAZIR && Color(g) != color) {
                scores[color] += wazir_move_bonus;
            }
        }
    }

    for (int c = 0; c < COLOR_COUNT; ++c) {
        for (int p = 0; p < PIECE_COUNT; ++p) {
            int n = state.captured[c][p];
            scores[c] += piece_values[p] * n * material_weight;
            // TODO: count all copies or just one?
            scores[c] += drop_move_value * n * empty;
        }
    }

    color_t player = state.NextPlayer();
    return scores[player] - scores[Other(player)];
}

// Evaluates an intermediate game state.
//
// Precondition: state.GameOver() == false
int Evaluate(const State &state) {
    return arg_complex_eval ? ComplexEval(state) : SimpleEval(state);
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
int Search(State &state, int depth_left, int ext_left, int alpha, int beta, SearchContext &ctx) {

    if (state.GameOver()) {
        int value = val_win + depth_left;
        return state.Winner() == state.NextPlayer() ? value : -value;
    }

    if (depth_left == 0) {
        if (ctx.evals_left <= 0) return 0;  // aborted
        --ctx.evals_left;
        return Evaluate(state);
    }

    // Query transposition table.
    TTEntry *entry = nullptr;
    if (depth_left >= arg_tt_depth) {
        entry = &tt[state];
        if (entry->depth_left > 0) ++ctx.tt_hits;
        // Note: we do use cached values with *higher* depth_left, which potentially
        // changes the result compared to not using the TT at all.
        // (May want to make this optional for deterministic testing.)
        if (entry->depth_left >= arg_tt_depth) {
            if (entry->lower_bound == entry->upper_bound ||
                    entry->lower_bound >= beta) {
                ++ctx.tt_used;
                return entry->lower_bound;
            } else if (entry->upper_bound <= alpha) {
                ++ctx.tt_used;
                return entry->upper_bound;
            }
        }
    }

    // Try all moves.
    int best_value = -val_inf;
    {
        Move moves[MAX_MOVES];
        size_t nmove = GenerateAllMoves(state, moves);
        int alpha2 = alpha;
        for (size_t i = 0; i < nmove; ++i) {
            UndoState undo = ExecuteMove(state, moves[i]);
            int d = depth_left - 1;
            int e = ext_left;
            if (d == 0 && undo.old_piece < PIECE_COUNT && e > 0) ++d, --e;
            int value = -Search(state, d, e, -beta, -alpha2, ctx);
            UndoMove(state, undo);
            if (ctx.evals_left <= 0) {
                // Search aborted. Return immediately so we DO NOT overwrite
                // the transposition table with an invalid value.
                return 0;
            }
            if (value > best_value) {
                best_value = value;
                if (value > alpha2) alpha2 = value;
                if (value >= beta) break;  // beta cut-off
            }
        }
    }

    // Update transposition table.
    if (entry != nullptr && entry->depth_left <= depth_left) {
        entry->lower_bound = best_value > alpha ? best_value : -val_inf;
        entry->upper_bound = best_value < beta  ? best_value : +val_inf;
        entry->depth_left = depth_left;
    }

    return best_value;
}

// Returns a list of best moves paired with the maximum game tree value.
// TODO: return result struct which includes depth
FindBestMovesResult FindBestMoves(State state, const std::vector<Move> &all_moves) {
    SearchContext ctx = {};
    ctx.evals_left = arg_max_evals;
    FindBestMovesResult res = {};
    // TODO: predict when the next depth will exceed evals_left
    assert(arg_max_depth >= 2);
    for (int depth = 2; depth <= arg_max_depth; ++depth) {
        std::vector<Move> best_moves;
        int best_value = -val_inf;
        int alpha = -val_inf;
        for (const Move &move : all_moves) {
            UndoState undo = ExecuteMove(state, move);
            int value = -Search(state, depth - 1, arg_search_ext, -val_inf, -alpha, ctx);
            UndoMove(state, undo);
            if (ctx.evals_left <= 0) {
                goto search_aborted;
            }
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
        res = FindBestMovesResult{
            .best_moves = std::move(best_moves),
            .best_value = best_value,
            .search_depth = depth,
            .nodes_evaluated = arg_max_evals - ctx.evals_left,
            .tt_hits = ctx.tt_hits,
            .tt_used = ctx.tt_used};
    }
search_aborted:
    return res;
}

PnsResult FindWinningMove(const State &state) {
    if (state.GameOver()) {
        PnsResult result;
        result.status = state.Winner() == state.NextPlayer() ? +1 : -1;
        return result;
    }
    if (arg_pns_max_nodes == 0) {
        return PnsResult{};
    }
    // Note this copies state, but that's okay.
    return ProofNumberSearch::Create(state).FindWinningMoves();
}
