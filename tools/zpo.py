# Implementation of the board game 0.1 for benefit of the arbiter.

from collections import Counter
from abc import ABC, abstractmethod

HEIGHT=8
WIDTH=8

ROW_COORDS = 'abcdefgh'
COL_COORDS = '12345678'

RED         = 0
BLUE        = 1
COLOR_COUNT = 2

WAZIR       = 0
KNIGHT      = 1
FERZ        = 2
DABBABA     = 3
ALFIL       = 4
PIECE_COUNT = 5

PIECE_MOVES   = [(0, 1), (1, 2), (1, 1), (0, 2), (2, 2)]
PIECE_IDS     = ['WNFDA', 'wnfda']
PIECE_COUNTS  = [1, 1, 2, 4, 8]
SETUP_COUNTS  = [Counter({ch: n for (ch, n) in zip(ids, PIECE_COUNTS)}) for ids in PIECE_IDS]


class ColoredPiece:
    def __init__(self, color, piece):
        self.color = color
        self.piece = piece

    def __repr__(self):
        return f'ColoredPiece({self.color}, {self.piece})'

    def __str__(self):
        return PIECE_IDS[self.color][self.piece]


class GameState:
    # The 8x8 board; each field is either empty (None) or occupied by a colored piece.
    fields: list[list[None|ColoredPiece]]

    # The current turn, counted from 0 (=number of turns played).
    # Determines the next player, and whether the game is in the setup or move phase.
    turn: int

    # For each player, and for each piece type, the number of pieces of that type on hand.
    # These pieces can be dropped back on the board during a player's turn.
    captured: list[list[int]]

    def __init__(self, max_turns = None):
        self.fields = [[None]*WIDTH for _ in range(HEIGHT)]
        self.turn = 0
        self.captured = [[0] * PIECE_COUNT for _ in range(COLOR_COUNT)]
        self.max_turns = max_turns

    def NextPlayer(self):
        return self.turn % COLOR_COUNT

    def IsGameOver(self):
        return (
            (self.max_turns is not None and self.turn >= self.max_turns) or
            any(self.captured[c][WAZIR] > 0 for c in range(COLOR_COUNT)))

    def Winner(self):
        for color in range(COLOR_COUNT):
            if self.captured[color][WAZIR] > 0:
                return color
        return None

    def DebugOutput(self) -> list[str]:
        lines = []
        lines.append(f'State after {self.turn} turns:')
        lines.append('  ' + ' '.join(COL_COORDS))
        for r, row in enumerate(self.fields):
            line = ' '.join(str(field) if field is not None else '.' for field in row)
            lines.append(f'{ROW_COORDS[r]} {line}')
        return lines


class Move(ABC):
    @abstractmethod
    def IsValid(self, state: GameState) -> bool:
        ...

    @abstractmethod
    def Execute(self, state: GameState) -> None:
        ...


class SetupMove(Move):
    def __init__(self, color, pieces):
        self.color = color
        self.pieces = pieces

    def __str__(self):
        ids = PIECE_IDS[self.color]
        return ''.join(ids[piece] for piece in self.pieces)

    def IsValid(self, state):
        return state.turn == self.color

    def Execute(self, state):
        for i, piece in enumerate(self.pieces, [0, 48][self.color]):
            state.fields[i // WIDTH][i % WIDTH] = ColoredPiece(self.color, piece)
        state.turn += 1


class MoveMove(Move):
    def __init__(self, r1, c1, r2, c2):
        self.r1 = r1
        self.c1 = c1
        self.r2 = r2
        self.c2 = c2

    def __str__(self):
        return ROW_COORDS[self.r1] + COL_COORDS[self.c1] + ROW_COORDS[self.r2] + COL_COORDS[self.c2]

    def IsValid(self, state):
        if state.turn < COLOR_COUNT:
            return False
        next_player = state.NextPlayer()
        src = state.fields[self.r1][self.c1]
        dst = state.fields[self.r2][self.c2]
        if src is None or src.color != next_player:
            return False
        if dst is not None and dst.color == next_player:
            return False
        dr = abs(self.r2 - self.r1)
        dc = abs(self.c2 - self.c1)
        if (min(dr, dc), max(dr, dc)) != PIECE_MOVES[src.piece]:
            return False
        return True

    def Execute(self, state):
        state.fields[self.r1][self.c1], state.fields[self.r2][self.c2], old = \
            None, state.fields[self.r1][self.c1], state.fields[self.r2][self.c2]
        if old is not None:
            state.captured[state.NextPlayer()][old.piece] += 1
        state.turn += 1


class DropMove(Move):
    def __init__(self, color, piece, r, c):
        self.color = color
        self.piece = piece
        self.r = r
        self.c = c

    def __str__(self):
        return PIECE_IDS[self.color][self.piece] + ROW_COORDS[self.r] + COL_COORDS[self.c]

    def IsValid(self, state):
        return (
            state.NextPlayer() == self.color and
            state.captured[self.color][self.piece] > 0 and
            state.fields[self.r][self.c] is None)

    def Execute(self, state):
        state.fields[self.r][self.c] = ColoredPiece(self.color, self.piece)
        state.captured[self.color][self.piece] -= 1
        state.turn += 1


def ParseMove(s):
    if len(s) == 16:
        counts = Counter(s)
        for color, correct_counts in enumerate(SETUP_COUNTS):
            if counts == correct_counts:
                ids = PIECE_IDS[color]
                return SetupMove(color, [ids.index(ch) for ch in s])
        return None

    if len(s) == 4:
        try:
            r1 = ROW_COORDS.index(s[0])
            c1 = COL_COORDS.index(s[1])
            r2 = ROW_COORDS.index(s[2])
            c2 = COL_COORDS.index(s[3])
            return MoveMove(r1, c1, r2, c2)
        except ValueError:
            return None

    if len(s) == 3:
        for color in range(COLOR_COUNT):
            piece = PIECE_IDS[color].find(s[0])
            if piece >= 0:
                break
        else:
            return None
        try:
            r = ROW_COORDS.index(s[1])
            c = COL_COORDS.index(s[2])
            return DropMove(color, piece, r, c)
        except ValueError:
            return None

    return None
