#!/usr/bin/env python3
#
# Tool to enumerate/generate start positions where alfils, dabbabas, and ferzes
# are independent: that is, they can reach disjoint subsets of the board, so
# that they never block each other, and one of each type can reach every square
# of the board.

from typing import Generator, cast

alfil_ids = [
  0, 1, 2, 3, 0, 1, 2, 3,
  4, 5, 6, 7, 4, 5, 6, 7,
]
dabbaba_ids = [
  0, 1, 0, 1, 0, 1, 0, 1,
  2, 3, 2, 3, 2, 3, 2, 3,
]
ferz_ids = [
  0, 1, 0, 1, 0, 1, 0, 1,
  1, 0, 1, 0, 1, 0, 1, 0,
]

def Search() -> Generator[str, None, None]:
    pieces: list[None|str] = [None]*16

    def MakeOptions(n, ids):
        options = [[] for _ in range(n)]
        for i, j in enumerate(ids):
            if pieces[i] is None:
                options[j].append(i)
        return options

    def PlaceAlfils():
        options = MakeOptions(8, alfil_ids)
        def Place(i):
            if i == 8:
                yield from PlaceDabbabas()
            else:
                for j in options[i]:
                    pieces[j] = 'A'
                    yield from Place(i + 1)
                    pieces[j] = None
        yield from Place(0)

    def PlaceDabbabas():
        options = MakeOptions(4, dabbaba_ids)
        def Place(i):
            if i == 4:
                yield from PlaceFerzes()
            else:
                for j in options[i]:
                    pieces[j] = 'D'
                    yield from Place(i + 1)
                    pieces[j] = None
        yield from Place(0)

    def PlaceFerzes():
        options = MakeOptions(2, ferz_ids)
        def Place(i):
            if i == 2:
                yield from PlaceKnight()
            else:
                for j in options[i]:
                    pieces[j] = 'F'
                    yield from Place(i + 1)
                    pieces[j] = None
        yield from Place(0)

    def PlaceKnight():
        for i, v in enumerate(pieces):
            if v is None:
                pieces[i] = 'N'
                yield from PlaceWazir()
                pieces[i] = None

    def PlaceWazir():
        for i, v in enumerate(pieces):
            if v is None:
                pieces[i] = 'W'
                yield ''.join(cast(list[str], pieces))
                pieces[i] = None

    yield from PlaceAlfils()


def Construct(index: int) -> str:
    '''Given an index between 0 and 2**15 (32768) exclusive, returns a distinct
       starting position with independent Alfils/Dababas/Ferzes.'''

    pieces: list[None|str] = [None]*16

    # This lists for each piece the possible fields indices.
    # Essentially, it's the inverse of alfil_id, dabbaba_id, and ferz_id
    # defined above.
    for ch, opts in [
        ('A', [  0,  4 ]),
        ('A', [  1,  5 ]),
        ('A', [  2,  6 ]),
        ('A', [  3,  7 ]),
        ('A', [  8, 12 ]),
        ('A', [  9, 13 ]),
        ('A', [ 10, 14 ]),
        ('A', [ 11, 15 ]),
        ('D', [  0,  2,  4,  6 ]),
        ('D', [  1,  3,  5,  7 ]),
        ('D', [  8, 10, 12, 14 ]),
        ('D', [  9, 11, 13, 15 ]),
        ('F', [  0,  2,  4,  6,  9, 11, 13, 15 ]),
        ('F', [  1,  3,  5,  7,  8, 10, 12, 14 ]),
        ('N', [  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15 ])
    ]:
        rem = [i for i in opts if pieces[i] is None]
        assert len(rem) == 2
        i = rem[index & 1]
        index >>= 1
        pieces[i] = ch

    # There should be exactly 1 space left for the Wazir
    i, = [i for i, v in enumerate(pieces) if v is None]
    pieces[i] = 'W'
    return ''.join(cast(list[str], pieces))

enumerated = list(Search())
generated  = list(map(Construct, range(2**15)))

assert sorted(generated) == sorted(enumerated)
assert len(set(generated)) == 2**15
