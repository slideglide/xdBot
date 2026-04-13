#pragma once

#include <Geode/Geode.hpp>
class Utils {
public:
    /* @brief Get the creation time of the macro file */
    static std::time_t getFileCreationTime(const std::filesystem::path& path);

    /* @brief Formats a time_t as local time (YYYY-MM-DD HH:MM:SS) */
    static std::string formatTime(std::time_t time);

    /* @brief Getter for the background texture */
    static std::string getTexture();

    /* @brief Helper to change the mod's background color */
    static void setBackgroundColor(geode::NineSlice* bg);

    /* @brief Get PauseLayer from the scene */
    static PauseLayer* getPauseLayer();
};