#include <Geode/Geode.hpp>

#include "gdr.hpp"

cocos2d::CCPoint dataFromString(std::string dataString) {
    auto parts = geode::utils::string::split(dataString, ",");
    float xPos = parts.size() > 1 ? geode::utils::numFromString<float>(parts[1]).unwrapOr(0.f) : 0.f;
    float yPos = parts.size() > 2 ? geode::utils::numFromString<float>(parts[2]).unwrapOr(0.f) : 0.f;
    return { xPos, yPos };
}

geode::prelude::VersionInfo getVersion(std::vector<std::string> nums) {
    if (nums.size() < 3) return geode::prelude::VersionInfo(0, 0, 0);
    size_t major = geode::utils::numFromString<int>(nums[0]).unwrapOr(0);
    size_t minor = geode::utils::numFromString<int>(nums[1]).unwrapOr(0);
    size_t patch = geode::utils::numFromString<int>(nums[2]).unwrapOr(0);
    return geode::prelude::VersionInfo(major, minor, patch);
}
