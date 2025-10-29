#!/usr/bin/env python3

# Evaluates the starting positions of Wazirs in winning games.
#
# The hypothesis is that it's beneficial to start with the Wazir on the
# back line and near the corner.
#
# This tool is meant to be run on a logdir of a large number of games
# of the player playing against itself, using randomized starting positions.

import glob
import os.path
import sys
import zpo

def Main(filenames):
    games  = [0]*64
    scores = [0]*64

    for filename in filenames:
        # Parse transcript and execute all moves to determine the winner
        with open(filename, 'rt') as f:
            moves = [zpo.ParseMove(line.strip()) for line in f]
            state = zpo.GameState()
            for move in moves:
                assert move.IsValid(state)
                move.Execute(state)
            winner = state.Winner()

        # Record position of wazir for winning player
        w0 = moves[0].pieces.index(zpo.WAZIR)
        w1 = 48 + moves[1].pieces.index(zpo.WAZIR)
        games[w0] += 1
        games[w1] += 1
        if winner is None:
            scores[w0] += 1
            scores[w1] += 1
        elif winner == 0:
            scores[w0] += 2
        elif winner == 1:
            scores[w1] += 2
        else:
            assert False

    # Format output: for each square of the board, the winrate when the Wazir
    # is placed on that square.
    s = ''
    for r in range(8):
        for c in range(8):
            if c > 0:
                s += ' '
            if games[8*r + c] == 0:
                s += '-----'
            else:
                s += '%5.3f' % (scores[8*r + c] / (2 * games[8*r + c]))
        s += '\n'
    print(s)

if __name__ == '__main__':
    if len(sys.argv) != 2:
        print(f'Usage: {sys.argv[0]} <logdir>', file=sys.stderr)
        sys.exit(1)
    logdir = sys.argv[1]
    if not os.path.isdir(logdir):
        print('Not a directory:', logdir, file=sys.stderr)
        sys.exit(1)
    transcripts = glob.glob(os.path.join(logdir, '*-transcript.txt'))
    if not transcripts:
        print('No transcripts in directory:', logdir, file=sys.stderr)
        sys.exit(1)
    Main(transcripts)
