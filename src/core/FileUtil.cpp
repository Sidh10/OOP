#include "quant/core/FileUtil.hpp"

#include <sys/stat.h>

#ifdef _WIN32
#include <direct.h>
#endif

namespace quant {

void ensureDirectory(const std::string& path) {
    struct stat st;
    if (stat(path.c_str(), &st) == 0) return;
#ifdef _WIN32
    _mkdir(path.c_str());
#else
    mkdir(path.c_str(), 0755);
#endif
}

}  // namespace quant
