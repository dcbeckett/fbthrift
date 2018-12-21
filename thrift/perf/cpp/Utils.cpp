#include "Utils.h"

#include <sys/utsname.h>
#include <folly/Conv.h>
#include <folly/String.h>
#include <fstream>
#include <string>

namespace loadgen_utils {

struct kernel_version {
    int major;
    int minor;
    int patch;
};

namespace {
kernel_version get_kernel_version() {
  utsname u;
  uname(&u);
  std::vector<folly::StringPiece> tokens;
  std::vector<int> inttokens;
  std::string rls = u.release;

  // Trim -localversion
  auto idx = rls.find('-');
  if (idx != std::string::npos) {
    rls = rls.substr(0, idx);
  }

  folly::split(".", rls, tokens);
  std::transform(
      begin(tokens), end(tokens), back_inserter(inttokens), [](auto piece) {
        return folly::to<int>(piece);
      });
  CHECK_GE(inttokens.size(), 3);

  return kernel_version{inttokens[0], inttokens[1], inttokens[2]};
}
    }
void verify_ktls_compatibility() {
  // Check for a supported kernel
  auto kv = get_kernel_version();
    if (kv.major < 4 || (kv.major == 4 && kv.minor < 16)) {
      LOG(WARNING)
          << "**** -enable_ktls specified, but requires at least 4.16 kernel. "
          << "Ensure your kernel supports TLS_RX and TLS_TX if not mainline";
    }

  // Check for the presence of the 'tls' module
  std::ifstream f("/proc/modules");
  bool found = false;
  while (f.good()) {
    char buf[64];
    f.getline(buf, sizeof(buf));
    if (std::memcmp(buf, "tls", 3)) {
        found = true;
        break;
    }
  }

  if (!found) {
    LOG(ERROR)
        << "**** 'tls' module not loaded in running kernel. -enable_ktls will most likely fail";
  }
}
} // namespace loadgen
