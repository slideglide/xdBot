#include "utils.hpp"

using namespace geode::prelude;

std::time_t Utils::getFileCreationTime(const std::filesystem::path& path) {
    if (!std::filesystem::exists(path)) return 0;
    
    auto ftime = std::filesystem::last_write_time(path);
    
    using namespace std::chrono;
    using file_clock = decltype(ftime)::clock;
    
    const auto file_time = time_point_cast<system_clock::duration>(
        ftime - file_clock::now() + system_clock::now()
    );
    
    return system_clock::to_time_t(file_time);
}

std::string Utils::formatTime(std::time_t time) {
    auto tm = geode::localtime(time);

    return fmt::format("{:%Y-%m-%d %H:%M:%S}", tm);
}

std::string Utils::getTexture() {
    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");
    std::string texture = color == ccc3(51, 68, 153) ? "GJ_square02.png" : "GJ_square06.png";
    
    return texture;
}

void Utils::setBackgroundColor(geode::NineSlice* bg) {
    cocos2d::ccColor3B color = Mod::get()->getSettingValue<cocos2d::ccColor3B>("background_color");
    
    if (color == ccc3(51, 68, 153)) {
        color = ccc3(255, 255, 255);
        bg->setColor(color);
    }
}

PauseLayer* Utils::getPauseLayer() {
    if (auto scene = CCScene::get()) 
        return scene->getChildByType<PauseLayer>(0);
    
    return nullptr;
}