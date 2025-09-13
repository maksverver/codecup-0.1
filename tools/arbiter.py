#!/usr/bin/env python3

# Arbiter for the 2026 CodeCup game 0.1 (Zero Point One)

import argparse
from contextlib import nullcontext
import concurrent.futures
from collections import Counter, defaultdict
from datetime import datetime
from enum import Enum
import functools
import io
import os.path
import subprocess
from subprocess import DEVNULL, PIPE
import shlex
import sys
import time

import zpo


class Outcome(Enum):
    WIN  = 1
    LOSS = 2
    TIE  = 3
    FAIL = 4  # crash, invalid move, etc.


class PlayerResult:
    def __init__(self, outcome, game_score, competition_score, time):
        self.outcome = outcome
        self.game_score = game_score
        self.competition_score = competition_score
        self.time = time


def CalcResults(state: zpo.GameState, times: list[float]) -> list[PlayerResult]:
    results: list[PlayerResult|None] = [None, None]
    if state.IsGameOver():
        # Game ended regularly.
        winner = state.Winner()
        for p in range(2):
            if winner is None:
                outcome = Outcome.TIE
                game_score = 1
                competition_score = 11
            elif winner == p:
                outcome = Outcome.WIN
                game_score = 2
                competition_score = 21
            else:
                outcome = Outcome.LOSS
                game_score = 0
                competition_score = 1
            results[p] = PlayerResult(outcome, game_score, competition_score, times[p])
    else:
        # Game ended irregularly, with the current player failing.
        #
        # Assign a token score to the winner (note: this is different from the
        # CodeCup rules where the game is finished with the arbiter playing randomly)
        failer = state.NextPlayer()
        winner = 1 - failer
        results[failer] = PlayerResult(Outcome.FAIL, 0,  0, times[failer])
        results[winner] = PlayerResult(Outcome.WIN,  0, 21, times[winner])
    return results  # type: ignore


def Launch(command: str, logfile: str|None):
    stderr = DEVNULL if logfile is None else open(logfile, 'wt')
    return subprocess.Popen(command, shell=True, text=True, stdin=PIPE, stdout=PIPE, stderr=stderr)


def RunGame(command1: str, command2: str, transcript: str|None, logfile1: str, logfile2: str):
    roles = ['First', 'Second']
    commands = [command1, command2]
    procs = [Launch(command1, logfile1), Launch(command2, logfile2)]
    times = [0.0, 0.0]

    state = zpo.GameState(max_turns=102)
    last_move = None

    with (open(transcript, 'wt') if transcript is not None else nullcontext()) as transcript_file:
        while not state.IsGameOver():
            player = state.NextPlayer()
            proc = procs[player]
            assert proc.stdin and proc.stdout

            # Give player input
            try:
                if last_move is None:
                    proc.stdin.write('Start\n')
                else:
                    proc.stdin.write(str(last_move) + '\n')
                proc.stdin.flush()
            except BrokenPipeError:
                break

            # Read player's move
            start_time = time.monotonic()
            line = proc.stdout.readline().strip()
            times[player] += time.monotonic() - start_time

            # Parse, validate move and execute move
            last_move = zpo.ParseMove(line)
            if last_move is None or not last_move.IsValid(state):
                if transcript_file:
                    # Include invalid move in transcript (for debugging)
                    print(f'# Invalid move: [{line}]', file=transcript_file)
                    print(f'# Parsed as:    [{last_move}]', file=transcript_file)
                    for line in state.DebugOutput():
                        print('#', line, file=transcript_file)
                break

            last_move.Execute(state)

            if transcript_file:
                print(str(last_move), file=transcript_file)

    results = CalcResults(state, times)

    # Gracefully quit. (Do this after computing results, because a program can
    # still fail if it doesn't exit cleanly, overriding the previous outcome.)
    for p in procs:
        if p.poll() is None:
            try:
                assert p.stdin
                p.stdin.write('Quit\n')
                p.stdin.flush()
            except BrokenPipeError:
                pass

    for i, p in enumerate(procs):
        status = p.wait()
        if status != 0:
            print(f'{roles[i]} command exited with nonzero status {status}: {commands[i]}', file=sys.stderr)
            results[i].outcome = Outcome.FAIL

    return results


def MakeLogfilenames(logdir, name1, name2, game_index, game_count):
    if not logdir:
        return (None, None, None)
    prefix = f'{name1}-vs-{name2}'
    if game_count > 1:
        prefix = f'game-{game_index + 1}-of-{game_count}-{prefix}'
    return (os.path.join(logdir, f'{prefix}-{role}.txt')
            for role in ('transcript', 'output-1', 'output-2'))


class Tee:
    '''Writes output to a file, and also to stderr.'''

    def __init__(self, file):
        self.file = file

    def __enter__(self):
        return self

    def __exit__(self, exception_type, exception_value, traceback):
        self.file.close()

    def close(self, s):
        self.file.close()

    def write(self, s):
        sys.stderr.write(s)
        return self.file.write(s)


def RunGames(commands, names, rounds, logdir, executor=None):
    P = len(commands)

    if rounds == 0:
        # Play only one match between ever pair of players, regardless of order
        pairings = [(i, j) for i in range(P) for j in range(i + 1, P)]
    else:
        # Play one match per round between every pair of players and each order
        pairings = [(i, j) for i in range(P) for j in range(P) if i != j] * rounds

    games_per_player = sum((i == 0) + (j == 0) for (i, j) in pairings)

    # Run all of the games, keeping track of total times and outcomes per player.
    player_time_total = [0.0]*P
    player_time_max   = [0.0]*P
    player_outcomes   = [defaultdict(lambda: 0) for _ in range(P)]
    player_scores     = [0]*P

    def AddStatistics(i, j, results):
        player_time_total[i] += results[0].time
        player_time_total[j] += results[1].time
        player_time_max[i] = max(player_time_max[i], results[0].time)
        player_time_max[j] = max(player_time_max[j], results[1].time)
        player_outcomes[i][results[0].outcome] += 1
        player_outcomes[j][results[1].outcome] += 1
        player_scores[i] += results[0].competition_score
        player_scores[j] += results[1].competition_score

    futures = []

    symlink_playerlogs = logdir and len(pairings) > 1

    if logdir:
        print('Writing logs to directory', logdir, file=sys.stderr)
        print(file=sys.stderr)

    if symlink_playerlogs:
        playerlog_dirs = []
        for name in names:
            dirname = os.path.join(logdir, name + '-logs')
            os.mkdir(dirname)
            playerlog_dirs.append(dirname)

    with (Tee(open(os.path.join(logdir, 'results.txt'), 'wt'))
                if logdir and len(pairings) > 1 else nullcontext()) as f:
        if f is None:
            f = sys.stderr

        print('Game Player 1           Player 2           Sc1 Sc2 Outc1 Outc2 Pts1 Pts2 Time 1 Time 2', file=f)
        print('---- ------------------ ------------------ --- --- ----- ----- ---- ---- ------ ------', file=f)

        def PrintRowStart(game_index, name1, name2):
            print('%4d %-18s %-18s ' % (game_index + 1, name1, name2), end='', file=f)
        def PrintRowFinish(results):
            p1, p2 = results
            print('%3d %3d %-5s %-5s %4d %4d %6.2f %6.2f' % (
                p1.game_score,
                p2.game_score,
                p1.outcome.name,
                p2.outcome.name,
                p1.competition_score,
                p2.competition_score,
                p1.time,
                p2.time), file=f)

        for game_index in range(len(pairings)):
            i, j = pairings[game_index]
            command1, command2 = commands[i], commands[j]
            name1, name2 = names[i], names[j]
            transcript, logfilename1, logfilename2 = MakeLogfilenames(logdir, name1, name2, game_index, len(pairings))

            if symlink_playerlogs:
                os.symlink(os.path.relpath(logfilename1, playerlog_dirs[i]), os.path.join(playerlog_dirs[i], os.path.basename(logfilename1)))  # type: ignore
                os.symlink(os.path.relpath(logfilename2, playerlog_dirs[j]), os.path.join(playerlog_dirs[j], os.path.basename(logfilename2)))  # type: ignore

            run = functools.partial(RunGame, command1, command2, transcript, logfilename1, logfilename2)

            if executor is None:
                # Run game on the main thread.
                # Print start of row first and flush, so the user can see which players
                # are competing while the game is in progress.
                PrintRowStart(game_index, name1, name2)
                sys.stderr.flush()
                results = run()
                PrintRowFinish(results)
                AddStatistics(i, j, results)
            else:
                # Start the game on a background thread.
                future = executor.submit(run)
                futures.append(future)

        # Print results from asynchronous tasks in the order they were started.
        #
        # This means that earlier tasks can block printing of results for later tasks
        # have have already finished, but this is better than printing the results out
        # of order.
        for game_index, future in enumerate(futures):
            i, j = pairings[game_index]
            results = future.result()
            AddStatistics(i, j, results)
            PrintRowStart(game_index, names[i], names[j])
            PrintRowFinish(results)

        print('---- ------------------ ------------------ ----- ----- ----- ----- ---- ---- ------ ------', file=f)

    # Print summary of players.
    if len(pairings) > 1:
        print(file=sys.stderr)
        with (Tee(open(os.path.join(logdir, 'summary.txt'), 'wt')) if logdir else nullcontext()) as f:
            if f is None:
                f = sys.stderr

            print('Player             Avg.Tm Max.Tm Tot. Wins Ties Loss Fail Points', file=f)
            print('------------------ ------ ------ ---- ---- ---- ---- ---- ------', file=f)
            for p in sorted(range(P), key=lambda p: (player_scores[p], player_outcomes[p][Outcome.WIN]), reverse=True):
                print('%-18s %6.2f %6.2f %4d %4d %4d %4d %4d %6d' %
                    ( names[p],
                        player_time_total[p] / games_per_player,
                        player_time_max[p],
                        games_per_player,
                        player_outcomes[p][Outcome.WIN],
                        player_outcomes[p][Outcome.TIE],
                        player_outcomes[p][Outcome.LOSS],
                        player_outcomes[p][Outcome.FAIL],
                        player_scores[p],
                    ), file=f)

            print('------------------ ------ ------ ---- ---- ---- ---- ---- ------', file=f)
            print('', file=f)
            print('Player command lines:', file=f)
            for i, (name, command) in enumerate(zip(names, commands), 1):
                print('%4d. %s: %s' % (i, name, command), file=f)

    if logdir:
        print(file=sys.stderr)
        print('Logs written to directory', logdir, file=sys.stderr)

    return player_scores


def DeduplicateNames(names):
    unique_names = []
    name_count = Counter(names)
    name_index = defaultdict(lambda: 0)
    for name in names:
        if name_count[name] == 1:
            unique_names.append(name)
        else:
            name_index[name] += 1
            unique_names.append(f'{name}-{name_index[name]}')
    return unique_names


def MakeLogdir(logdir, rounds):
    if logdir is None:
        return None

    assert os.path.isdir(logdir)
    # Create a subdirectory based on current date & time
    dirname = datetime.now().strftime('%Y%m%dT%H%M%S')
    logdir = os.path.join(logdir, dirname)
    os.mkdir(logdir)  # fails if path already exists
    return logdir


def Main():
    parser = argparse.ArgumentParser(prog='arbiter.py')
    parser.add_argument('command1', type=str,
            help='command to execute for player 1')
    parser.add_argument('command2', type=str,
            help='command to execute for player 2')
    parser.add_argument('commandN', type=str, nargs='*',
            help='command to execute for player N')
    parser.add_argument('--rounds', type=int, default=0,
            help='number of full rounds to play (or 0 for a single game)')
    parser.add_argument('--logdir', type=str, default=None,
            help='directory where to write player logs')
    parser.add_argument('-t', '--threads', type=int, default=0,
            help='parallelize execution using threads (no more than physical cores!)')

    args = parser.parse_args()
    logdir = MakeLogdir(args.logdir, args.rounds)

    commands = [args.command1, args.command2] + args.commandN

    names = DeduplicateNames([os.path.basename(shlex.split(command)[0]) for command in commands])

    with (concurrent.futures.ThreadPoolExecutor(max_workers=args.threads)
                if args.threads > 0 else nullcontext()) as executor:
        RunGames(commands, names, rounds=args.rounds, logdir=logdir, executor=executor)

if __name__ == '__main__':

    # Hack to fix spurious errors about broken pipes:
    #
    # Exception ignored in: <_io.TextIOWrapper name=8 encoding='UTF-8'>
    # Traceback (most recent call last):
    #   File "/home/maks/Code/CodeCup/2026-0.1/tools/arbiter.py", line 255, in RunGames
    #     results = run()
    # BrokenPipeError: [Errno 32] Broken pipe
    #
    def HandleUnraisable(unraisable):
        if unraisable.exc_type == BrokenPipeError and type(unraisable.object) == io.TextIOWrapper:
            return  # ignore
        sys.__unraisablehook__(unraisable)

    sys.unraisablehook = HandleUnraisable

    Main()
