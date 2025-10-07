
BASE64_DIGITS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_'

class Encoder:
    nbit: int
    bits: int

    def __init__(self):
        self.bits = 0
        self.nbit = 0

    def add_bit(self, i: int):
        self.bits |= i << self.nbit
        self.nbit += 1

    def add_bits(self, i: int, n: int):
        self.bits |= i << self.nbit
        self.nbit += n

    def add_unary_int(self, i: int):
        self.bits |= 1 << (self.nbit + i)
        self.nbit += i + 1

    def add_last_int(self, i: int):
        self.bits |= i << self.nbit
        self.nbit += max(i.bit_length(), 1)

    def finish(self):
        bits = self.bits
        nbit = self.nbit
        res = []
        while nbit > 0:
            res.append(BASE64_DIGITS[bits & 63])
            bits >>= 6
            nbit -= 6
        return ''.join(res)
