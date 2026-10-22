#pragma once
#include "ProfileStore.h"
namespace mlash {
// Bounded metadata-only comparison. Never returns YAML, credentials or node definitions.
class VersionChanges final {
public:
    static Result inspect(const ProfileStore& store,const Profile& profile);
};
}
