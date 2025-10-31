#!/usr/bin/env python3

# Performs a binomial test to calculate which version of a player is better.
#
# To use this, run a competition between exactly two players, then call this
# script with number of wins, ties, and losses, to calculate the probability
# that one player is better than the other. Example:
#
#   % tools/binomial-test.py 35 1 14
#   Player 1 is better than Player 2 (p=0.00330022, winrate=0.700000, n=50,
#   95% CI=[0.554,0.805])
#
# The p-value is the probability that the null-hypothesis holds, assuming
# that the players are equally matched, so lower p-values mean greater
# confidence in the claim that player 1 is better than player 2.
#
# This does a one-tailed test (i.e., it asks: what is the probability that
# the number of wins is at least as large as this?) (Maybe we should do a
# two-tailed test instead?)
#
# It also prints the 95% confidence interval of the true winrate (i.e.
# probability that player 1 defeats player 2, when they do not tie).
#
# More specifically, the interval is such that the odds of the true probability
# being lower than the lower bound are 2.5% and likewise for the upper bound.
# That means if the lower bound > 0.5 then p < 0.025 and vice versa.

from functools import cache
import sys

@cache
def nCr(n, r):
    '''Computes the bionial coefficient n over r, which is the number of subsets
        of size `r` of a set of `n`.'''
    assert 0 <= r <= n
    return 1 if r == 0 or r == n else nCr(n - 1, r - 1) + nCr(n - 1, r)

def CalcProb(wins, games, win_prob):
    '''Calculates the probability of having at least `wins` wins out of a total
       of `games` games, assuming that the probability of winning a match is
       `win_prob`.'''
    assert 0 <= wins <= games
    return sum(nCr(games, w) * win_prob**w * (1 - win_prob)**(games - w)
        for w in range(wins, games + 1))

def BinarySearch(lo, hi, steps, test):
    for _ in range(steps):
        mid = lo + (hi - lo) / 2
        if test(mid):
            hi = mid
        else:
            lo = mid
    return mid

def CalcCI(wins, games, confidence=0.95):
    lo_prob = (1.0 - confidence) / 2  # e.g. 0.025
    hi_prob = lo_prob + confidence    # e.g. 0.975
    lo_bound = BinarySearch(0.0, 1.0, 100, lambda v: CalcProb(wins, games, v) >= lo_prob)
    hi_bound = BinarySearch(0.0, 1.0, 100, lambda v: CalcProb(wins, games, v) >= hi_prob)
    return (lo_bound, hi_bound)

def Main(wins, ties, losses):
    # Divide ties between both players (is this valid?)
    wins1 = wins + ties // 2
    wins2 = losses + (ties - ties // 2)
    confidence = 0.95
    name1, name2 = 'Player 1', 'Player 2'
    if wins1 < wins2:
        wins1, wins2 = wins2, wins1
        name1, name2 = name2, name1
    games = wins1 + wins2
    p = CalcProb(wins1, games, 0.5)
    CalcCI(wins1, games)
    ci_lo, ci_hi = CalcCI(wins1, games, confidence)
    verdict = 'is better than' if wins1 != wins2 else 'is tied with'
    print(
        '%s %s %s' % (name1, verdict, name2),
        '(p=%g, winrate=%f, n=%d, %d%% CI=[%.3f,%.3f])' %
            (p, wins1 / games, games, confidence * 100, ci_lo, ci_hi))

if __name__ == '__main__':
    if len(sys.argv) != 4:
        print('Usage', sys.argv[0], '<wins>', '<ties>', '<losses>')
        sys.exit(1)

    sys.setrecursionlimit(25000)
    wins, ties, losses = map(int, sys.argv[1:])
    Main(wins, ties, losses)
