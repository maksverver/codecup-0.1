#ifndef CODEC_H_INCLUDED
#define CODEC_H_INCLUDED

#include "state.h"

#include <optional>
#include <string_view>

std::optional<State> DecodeCompactState(std::string_view s);

#endif  // ndef CODEC_H_INCLUDED
