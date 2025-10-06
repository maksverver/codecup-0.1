#include "analysis.h"

#include <cstring>
#include <vector>
#include <utility>

#include "options.h"

DECLARE_OPTION(int, arg_depth, 4, "depth", "Maximum search depth.");

DECLARE_OPTION(int, arg_tt_depth, 99, "tt-depth", "Minimum search depth left to use transposition table. (0 disables)");

DECLARE_OPTION(int, arg_pns_max_nodes, 0, "pns-max-nodes", "Maximum number of PNS nodes (in millions)");

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

static long long nodes_evaluated = 0;
static long long tt_hits = 0;
static long long tt_used = 0;

}  // namespace

// Evaluates an intermediate game state.
//
// Precondition: state.GameOver() == false
int Evaluate(const State &state) {
    int value = state.scores[0] - state.scores[1];
    return state.NextPlayer() == 0 ? value : -value;
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
        ++nodes_evaluated;
        return Evaluate(state);
    }

    // Query transposition table.
    TTEntry *entry = nullptr;
    if (depth_left >= arg_tt_depth) {
        entry = &tt[state];
        if (entry->depth_left > 0) ++tt_hits;
        // Note: we do use cached values with *higher* depth_left, which potentially
        // changes the result compared to not using the TT at all.
        // (May want to make this optional for deterministic testing.)
        if (entry->depth_left >= arg_tt_depth) {
            if (entry->lower_bound == entry->upper_bound ||
                    entry->lower_bound >= beta) {
                ++tt_used;
                return entry->lower_bound;
            } else if (entry->upper_bound <= alpha) {
                ++tt_used;
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
            int value = -Search(state, depth_left - 1, -beta, -alpha2);
            UndoMove(state, undo);
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

    if (false) {
        std::cerr
            << "nodes_evaluated=" << nodes_evaluated << ' '
            << "tt_hits=" << tt_hits << ' '
            << "tt_used=" << tt_used << std::endl;
    }

    return {best_moves, best_value};
}

// Proof number search
//
// TODO's:
//
//   - support max depth (based on turns left?); is also a partial solution
//     for repetitions.
//
//   - need to deal with repetitions and/or transpositions (but be careful!)
//     (especially repetitions, to avoid getting stuck in a long narrow path)
//
//   - maybe: initialize proof number of new nodes to the number of moves?
//     (might save either memory or time, but not a lot)
//
//   - instead of clearing the search tree, reuse parts of it between turns?
//     (can do a DFS up to depth 2 to find the successor state, then copy the
//     used parts of the tree)
//
//   - abort search based on a root proof number threshold?
//

namespace {

const int pn_inf = 999999999;

struct PnsNode {
    Move last_move = Move::Null();
    int pn = 1, dn = 1;  // proof/disproof numbers
    int children_begin = 0, children_end = 0;

    static PnsNode Create(color_t player, const State &state, Move last_move) {
        PnsNode node;
        node.last_move = last_move;
        if (state.GameOver()) {
            if (state.Winner() == player) {
                // Proven
                node.pn = 0;
                node.dn = pn_inf;
            } else {
                // Disproven
                node.pn = pn_inf;
                node.dn = 0;
            }
        }
        return node;
    }

    bool IsExpanded()  const { return children_begin > 0; }
    bool IsProven()    const { return pn == 0; }
    bool IsDisproven() const { return dn == 0; }
    bool IsFixed()     const { return IsProven() || IsDisproven(); }

    std::span<const PnsNode> Children(std::span<const PnsNode> nodes) const {
        return std::span(nodes.data() + children_begin, nodes.data() + children_end);
    }

    // Recalculates the proof/disproof numbers and returns the most provable child
    // substree index at the same time. (If the node is fixed, the result may be 0
    // instead.)
    template <bool my_turn>
    int Recalculate(std::span<const PnsNode> nodes) {
        int best_i = 0;
        int min = pn_inf;
        int sum = 0;
        for (int i = children_begin; i < children_end; ++i) {
            const PnsNode &child = nodes[i];
            int to_min = my_turn ? child.pn : child.dn;
            if (to_min < min) {
                min = to_min;
                best_i = i;
            }
            int to_sum = my_turn ? child.dn : child.pn;
            if (to_sum == pn_inf) {
                sum = pn_inf;
                break;
            }
            sum += to_sum;
        }
        pn = my_turn ? min : sum;
        dn = my_turn ? sum : min;
        return best_i;
    }

    template <bool my_turn>
    int MostProvingChild(std::span<const PnsNode> nodes) {
        int best_v = pn_inf;
        int best_i = 0;
        for (int i = children_begin; i != children_end; ++i) {
            int v = my_turn ? nodes[i].pn : nodes[i].dn;
            if (v < best_v) {
                best_v = v;
                best_i = i;
            }
        }
        assert(best_i > 0);
        return best_i;
    }
};

static_assert(sizeof(PnsNode) == 20);

class ProofNumberSearch {
public:
    ProofNumberSearch(const State &state, int max_moves) :
            root_state(state),  // temp, for debugging
            state(state),
            player(state.NextPlayer())
    {
        assert(!state.GameOver());
        assert(max_moves > 0);
        nodes.reserve(max_moves);
        nodes.push_back(PnsNode::Create(player, state, Move::Null()));  // root
    }

    PnsResult FindWinningMoves() {
        assert(!nodes.empty());
        const PnsNode &root = nodes.front();
        while (!root.IsFixed()) {
            if (!ExpandMostProving<true>(0)) break;
        }

        if (false) PrintTree(std::cerr, true);  // debug print tree

        PnsResult result;
        if (!root.IsFixed()) {
            result.status = 0;
        } else if (root.IsProven()) {
            result.status = 1;
            for (const PnsNode &child : root.Children(nodes)) {
                if (child.IsProven()) {
                    result.winning_move = child.last_move;
                    break;
                }
            }
        } else {  // disproven
            result.status = -1;
        }
        result.nodes_expanded = nodes.size();
        return result;
    }

private:
    State root_state;  // temp, for debugging!
    State state;
    color_t player;
    std::vector<PnsNode> nodes;

    // Returns true when updated, false when we ran out of move budget.
    template<bool my_turn>
    bool ExpandMostProving(int node_index) {
        PnsNode &node = nodes[node_index];
        assert(!node.IsFixed());

        if (false) {
            std::cerr << "ExpandMostProving(" << node_index << ") "
                 << "type=" << (my_turn == player ? "OR" : "AND")
                 << ' ' << node.pn << '/' << node.dn << "\n";
        }

        if (!node.IsExpanded()) {
            Move moves[MAX_MOVES];
            size_t nmove = GenerateAllMoves(state, moves);
            if (nodes.capacity() < nodes.size() + nmove) {
                // Memory limit reached.
                return false;
            }
            // TODO: optimize away this copying of moves...?
            node.children_begin = nodes.size();
            node.children_end = nodes.size() + nmove;
            for (size_t i = 0; i < nmove; ++i) {
                UndoState undo_state = ExecuteMove(state, moves[i]);
                nodes.push_back(PnsNode::Create(player, state, moves[i]));
                UndoMove(state, undo_state);
            }
            if (nmove == 0) {
                // Treat stalemate as not won by either player, regardless of what the
                // Caia referee does. (Should be extremely rare in practice anyway).
                node.pn = pn_inf;
                node.dn = 0;
            } else {
                node.Recalculate<my_turn>(nodes);
            }
            // Return to parent since current node's pn/dn changed (except in the
            // rare case where we had only a single child, but it's safe to return to
            // the parent regardless)
            return true;
        }

        // Find the most proving child subtree.
        int best_i = node.MostProvingChild<my_turn>(nodes);
        UndoState undo_state = ExecuteMove(state, nodes[best_i].last_move);
        bool success = true;
        for (;;) {
            success = ExpandMostProving<!my_turn>(best_i);
            if (!success) break;
            int old_pn = node.pn;
            int old_dn = node.dn;
            int new_best_i = node.Recalculate<my_turn>(nodes);
            // Return to parent if the current node's pn/dn changed, which means
            // the subtree may no longer be the most proving one. We break here
            // instead of returning to make sure the last move is undone.
            if (node.pn != old_pn || node.dn != old_dn) break;
            assert(new_best_i != 0);

            // Check if the most proving child subtree has changed.
            if (new_best_i != best_i) {
                UndoMove(state, undo_state);
                best_i = new_best_i;
                undo_state = ExecuteMove(state, nodes[best_i].last_move);
            }
        }
        UndoMove(state, undo_state);
        return success;
    }

    // Debug-prints the entire tree from the root (no matter what the current
    // state is).
    void PrintTree(std::ostream &os, bool fixed_only=false) {
        if (nodes.empty()) return;
        State copy = root_state;
        PrintTree(os, copy, 0, 0, fixed_only);
        assert(copy == root_state);
    }

    // Debug-prints a subtree. `state` must match the state at index `node_index`
    // and is modified during execution but restored to the original state on
    // return.
    void PrintTree(std::ostream &os, State &state, int node_index, int depth = 0, bool fixed_only=false) {
        const PnsNode &node = nodes[node_index];
        std::string indent(depth*2, ' ');
        os << "[" << node_index << "] "
                << (player == state.NextPlayer() ? "OR" : "AND")
                << " p/d=" << node.pn << "/" << node.dn << '\n';
        for (int i = node.children_begin; i != node.children_end; ++i) {
            if (fixed_only && !nodes[i].IsFixed()) continue;
            os << indent << "- " << FormatMove(state.NextPlayer(), nodes[i].last_move);
            UndoState undo_state = ExecuteMove(state, nodes[i].last_move);
            PrintTree(os, state, i, depth + 1, fixed_only);
            UndoMove(state, undo_state);
        }
    }
};

}  // namespace

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
    assert(std::numeric_limits<int>::max() / 1000000 >= arg_pns_max_nodes);
    ProofNumberSearch search(state, arg_pns_max_nodes * 1000000);
    return search.FindWinningMoves();
}
