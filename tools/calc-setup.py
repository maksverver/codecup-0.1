#!/usr/bin/env python3

# Counts the total number of starting positions where ferzes, dabbabas and alfils
# can reach disjoint subsets of the board.

from functools import cache

alfil_id = [
  0, 1, 2, 3, 0, 1, 2, 3,
  4, 5, 6, 7, 4, 5, 6, 7,
]
dabbaba_id = [
  0, 1, 0, 1, 0, 1, 0, 1,
  2, 3, 2, 3, 2, 3, 2, 3,
]
ferz_id = [
  0, 1, 0, 1, 0, 1, 0, 1,
  1, 0, 1, 0, 1, 0, 1, 0,
]

@cache
def Calc(pos, wazirs, knights, ferzes, dababbas, alfils, ferz_occ, dabbaba_occ, alfil_occ):
    if wazirs + knights + ferzes + dababbas + alfils == 0:
       return 1

    res = 0
    if wazirs > 0:
       res += Calc(pos + 1, wazirs - 1, knights, ferzes, dababbas, alfils, ferz_occ, dabbaba_occ, alfil_occ)
    if knights > 0:
       res += Calc(pos + 1, wazirs, knights - 1, ferzes, dababbas, alfils, ferz_occ, dabbaba_occ, alfil_occ)
    if ferzes > 0 and (ferz_occ & (1 << ferz_id[pos])) == 0:
       res += Calc(pos + 1, wazirs, knights, ferzes - 1, dababbas, alfils, ferz_occ | (1 << ferz_id[pos]), dabbaba_occ, alfil_occ)
    if dababbas > 0 and (dabbaba_occ & (1 << dabbaba_id[pos])) == 0:
       res += Calc(pos + 1, wazirs, knights, ferzes, dababbas - 1, alfils, ferz_occ, dabbaba_occ  | (1 << dabbaba_id[pos]), alfil_occ)
    if alfils > 0 and (alfil_occ & (1 << alfil_id[pos])) == 0:
       res += Calc(pos + 1, wazirs, knights, ferzes, dababbas, alfils - 1, ferz_occ, dabbaba_occ, alfil_occ | (1 << alfil_id[pos]))
    return res

print(Calc(0, 1, 1, 2, 4, 8, 0, 0, 0))
