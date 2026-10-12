#pragma once

#include <string>
#include <vector>
#include <cstdint>
#include <sstream>

struct PlayerAction {
    size_t frame = 0;
    float xPos = 0.0f;
    int button = 1;       // 1 = Jump/Primary, 2 = Left, 3 = Right
    bool isPush = true;   // true = push, false = release
    bool isPlayer2 = false;
    float yPos = 0.0f;    // Optional exact Y coordinate from 240Hz frameFixes

    bool operator==(const PlayerAction& other) const {
        return frame == other.frame &&
               button == other.button &&
               isPush == other.isPush &&
               isPlayer2 == other.isPlayer2;
    }
};

struct MacroData {
    int levelId = 0;
    std::string levelName = "";
    float completionPercentage = 0.0f;
    size_t totalFrames = 0;
    int attemptsToSolve = 0;
    std::vector<PlayerAction> actions;

    std::string serialize() const {
        std::ostringstream ss;
        ss << "{\n";
        ss << "  \"levelId\": " << levelId << ",\n";
        ss << "  \"levelName\": \"" << levelName << "\",\n";
        ss << "  \"completionPercentage\": " << completionPercentage << ",\n";
        ss << "  \"totalFrames\": " << totalFrames << ",\n";
        ss << "  \"attemptsToSolve\": " << attemptsToSolve << ",\n";
        ss << "  \"actions\": [\n";
        for (size_t i = 0; i < actions.size(); ++i) {
            const auto& a = actions[i];
            ss << "    {\"frame\": " << a.frame 
               << ", \"x\": " << a.xPos
               << ", \"y\": " << a.yPos
               << ", \"button\": " << a.button 
               << ", \"push\": " << (a.isPush ? "true" : "false")
               << ", \"p2\": " << (a.isPlayer2 ? "true" : "false") << "}";
            if (i + 1 < actions.size()) {
                ss << ",";
            }
            ss << "\n";
        }
        ss << "  ]\n";
        ss << "}\n";
        return ss.str();
    }

    static MacroData deserialize(const std::string& jsonStr) {
        MacroData macro;
        auto findInt = [&](const std::string& key) -> int {
            auto pos = jsonStr.find("\"" + key + "\"");
            if (pos == std::string::npos) return 0;
            pos = jsonStr.find(':', pos);
            if (pos == std::string::npos) return 0;
            return std::stoi(jsonStr.substr(pos + 1));
        };

        auto findFloat = [&](const std::string& key) -> float {
            auto pos = jsonStr.find("\"" + key + "\"");
            if (pos == std::string::npos) return 0.0f;
            pos = jsonStr.find(':', pos);
            if (pos == std::string::npos) return 0.0f;
            return std::stof(jsonStr.substr(pos + 1));
        };

        auto findString = [&](const std::string& key) -> std::string {
            auto pos = jsonStr.find("\"" + key + "\"");
            if (pos == std::string::npos) return "";
            pos = jsonStr.find('\"', pos + key.length() + 2);
            if (pos == std::string::npos) return "";
            auto endPos = jsonStr.find('\"', pos + 1);
            if (endPos == std::string::npos) return "";
            return jsonStr.substr(pos + 1, endPos - pos - 1);
        };

        macro.levelId = findInt("levelId");
        macro.levelName = findString("levelName");
        macro.completionPercentage = findFloat("completionPercentage");
        macro.totalFrames = static_cast<size_t>(findInt("totalFrames"));
        macro.attemptsToSolve = findInt("attemptsToSolve");

        auto arrPos = jsonStr.find("\"actions\"");
        if (arrPos != std::string::npos) {
            auto startBracket = jsonStr.find('[', arrPos);
            auto endBracket = jsonStr.find(']', startBracket);
            if (startBracket != std::string::npos && endBracket != std::string::npos) {
                size_t curr = startBracket;
                while (true) {
                    auto objStart = jsonStr.find('{', curr);
                    if (objStart == std::string::npos || objStart > endBracket) break;
                    auto objEnd = jsonStr.find('}', objStart);
                    if (objEnd == std::string::npos || objEnd > endBracket) break;
                    std::string item = jsonStr.substr(objStart, objEnd - objStart + 1);
                    
                    PlayerAction act;
                    auto fPos = item.find("\"frame\":");
                    if (fPos != std::string::npos) act.frame = std::stoull(item.substr(fPos + 8));
                    auto xPos = item.find("\"x\":");
                    if (xPos != std::string::npos) act.xPos = std::stof(item.substr(xPos + 4));
                    auto yPos = item.find("\"y\":");
                    if (yPos != std::string::npos) act.yPos = std::stof(item.substr(yPos + 4));
                    auto bPos = item.find("\"button\":");
                    if (bPos != std::string::npos) act.button = std::stoi(item.substr(bPos + 9));
                    auto pPos = item.find("\"push\":");
                    if (pPos != std::string::npos) act.isPush = (item.find("true", pPos) != std::string::npos && item.find("true", pPos) < item.find(',', pPos));
                    auto p2Pos = item.find("\"p2\":");
                    if (p2Pos != std::string::npos) act.isPlayer2 = (item.find("true", p2Pos) != std::string::npos);

                    macro.actions.push_back(act);
                    curr = objEnd + 1;
                }
            }
        }
        return macro;
    }
};
