#pragma once

#include <cstddef>

#include "net/stats_types.h"

namespace cyd {

// parse_stats fills `out` from a UTF-8 JSON buffer. Returns false on malformed
// input or schema mismatch (schema != 1). Missing optional fields default to
// zero / empty string rather than failing.
bool parse_stats(const char *data, size_t len, Stats &out);

} // namespace cyd
