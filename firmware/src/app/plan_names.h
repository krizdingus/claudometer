#pragma once

#include <string>

namespace cyd {

// Maps daemon plan slug ("max-20x", "pro", …) to display form ("MAX 20x", "PRO").
// Returns the raw c_str() unchanged when no mapping is known.
inline const char *pretty_plan_str(const std::string &raw) {
  if (raw == "max-20x") return "MAX 20x";
  if (raw == "max-5x")  return "MAX 5x";
  if (raw == "pro")     return "PRO";
  if (raw == "free")    return "FREE";
  return raw.c_str();
}

}  // namespace cyd
