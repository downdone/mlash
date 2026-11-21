file(SHA256 "${HELPER}" sha)
file(WRITE "${OUTPUT}" "#pragma once\n#define MLASH_WIREGUARD_SHA256 \"${sha}\"\n")
