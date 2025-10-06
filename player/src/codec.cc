#include "codec.h"

namespace {

constexpr std::string_view BASE64_DIGITS =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

// Utility class to decode a base64-encoded string into a sequence of bits.
class Decoder {
public:
    Decoder(std::string_view s) : chars(s) {}

    bool Failed() const {
        return failed;
    }

    // Returns the maximum number of bits left (assuming there are no failures)
    size_t BitsLeft() const {
        return 6*(chars.size() - pos) + nbit;
    }

    // Returns the next bit, or 0 if reading failed.
    bool GetBit() {
        if (nbit == 0 && !Refill()) return 0;
        --nbit;
        bool res = bits & 1;
        bits >>= 1;
        return res;
    }

    // Returns the next `n` bits as an integer in lsb order. If reading failed,
    // the missing bits are set to 0.
    unsigned GetBits(int n) {
        assert(n <= std::numeric_limits<unsigned>::digits);
        unsigned res = 0;
        int i = 0;
        while (n > nbit) {
            res |= bits << i;
            i += nbit;
            n -= nbit;
            bits = 0;
            nbit = 0;
            Refill();
        }
        // n <= 6
        res |= (bits & ((1u << n) - 1)) << i;
        bits >>= n;
        nbit -=  n;
        return res;
    }

    // Returns a number i, read in unary format, consisting of `i` 0s in a row
    // followed by a single 1. If reading fails, returns (unsigned) -1, which is
    // the largest value that fits unsigned.
    unsigned GetUnary() {
        unsigned i = 0;
        while (!GetBit()) {
            if (failed) return static_cast<unsigned>(-1);
            ++i;
        }
        return i;
    }

private:
    // Parses the next characters in `chars` and puts the corresponding 6 bits
    // into the bit buffer. This overwrites whatever bits were left.
    //
    // Sets the failed bit if no characters are left, or the next character is
    // not a valid base64 digit.
    bool Refill() {
        if (failed) return false;

        if (pos == chars.size()) {
            failed = true;  // read past end of string
            return false;
        }

        size_t i = BASE64_DIGITS.find(chars[pos]);
        if (i >= BASE64_DIGITS.size()) {
            failed = true;  // invalid character
            return false;
        }

        bits = i;
        nbit = 6;
        ++pos;
        return true;
    }

    // Bit buffer: `bits` contains `nbit` bits converted from chars.
    unsigned bits = 0;
    int nbit = 0;

    // Input: `chars` is a string of base64 digits, and `pos` is the index of
    // the first unused character.
    std::string_view chars;
    size_t pos = 0;

    // Failed flag. This is set whenever a read error occurs. This flag is
    // sticky: once it is set, it will remain set, and all subsequent read
    // operations will fail.
    bool failed = false;
};

}  // namespace

std::optional<State> DecodeCompactState(std::string_view s) {
    Decoder dec(s);

    // Pieces on board.
    field_t fields[FIELD_COUNT];
    for (field_t &field : fields) {
        if (dec.GetBit() == 0) {
            field = EMPTY_FIELD;
        } else {
            color_t color = dec.GetBit() == 0 ? RED : BLUE;
            piece_t piece =
                dec.GetBit() == 1 ? ALFIL   :
                dec.GetBit() == 1 ? DABBABA :
                dec.GetBit() == 1 ? FERZ    :
                dec.GetBit() == 1 ? KNIGHT  : WAZIR;
            field = Field(color, piece);
        }
    }

    // Captured pieces.
    uint8_t captured[2][PIECE_COUNT] = {};
    for (int c = 0; c < COLOR_COUNT; ++c) {
        for (int p = 0; p < PIECE_COUNT; ++p) {
            captured[c][p] = dec.GetUnary();
        }
    }

    // Turn.
    size_t n = dec.BitsLeft();
    if (n < 1 || n > 31) return {};  // invalid number of bits for turn count
    int turn = dec.GetBits(n);

    if (dec.Failed()) return {};
    return State::Create(fields, captured, turn);
}
