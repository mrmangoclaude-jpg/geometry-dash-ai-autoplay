#pragma once

#include <string>
#include <ostream>

class GJBaseGameLayer;

namespace LevelExporter {
    void writeObjRects(std::ostream& rf, GJBaseGameLayer* layer);
    std::string exportLevelCsv(GJBaseGameLayer* layer);
    float getMaxLevelX(GJBaseGameLayer* layer);
}
