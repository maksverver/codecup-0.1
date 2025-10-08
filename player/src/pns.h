// Proof Number Search implementation
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

#ifndef PNS_H_INCLUDED
#define PNS_H_INCLUDED

#include "state.h"

#include <iostream>  // for debug printing

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

struct PnsResult {
    // +1 if proven win, -1 if proven loss, 0 if the search was aborted.
    int status = 0;

    // Winning move (if status == +1)
    Move winning_move = Move::Null();

    // Informative; number of nodes expanded during the search.
    int nodes_expanded = 0;
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
        assert(state == root_state);
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

#endif  // ndef PNS_H_INCLUDED
