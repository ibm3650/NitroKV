#include <fmt/core.h>

#include "nitrokv/common/version.hpp"

int main() {
    fmt::print(
        "nitrokv {}.{}.{}\n",
        nitrokv::common::version_major,
        nitrokv::common::version_minor,
        nitrokv::common::version_patch
    );
    return 0;
}
