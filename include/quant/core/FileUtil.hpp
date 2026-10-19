#pragma once

#include <string>

namespace quant {

// Creates the folder if it does not exist (one level). Silent if it already exists.
void ensureDirectory(const std::string& path);

}  // namespace quant
