#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/binding/PlayerObject.hpp>
#include <Geode/binding/GameObject.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPlayLayer, PlayLayer) {
    struct Fields {
        GameObject* anticheatSpike = nullptr;
    };

    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        m_fields->anticheatSpike = nullptr;
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) {
            return false;
        }

        if (level) {
            int levelId = level->m_levelID;
            bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
            if (MacroManager::get().hasSavedMacro(levelId, collectCoins)) {
                Notification::create("AI AutoPlay: Saved run available! (Press 'G' to Play)", NotificationIcon::Info)->show();
                
                bool autoPlay = Mod::get()->getSettingValue<bool>("auto_play_saved");
                if (autoPlay) {
                    auto saved = MacroManager::get().loadMacro(levelId, collectCoins);
                    if (saved.has_value()) {
                        SolverEngine::get().startPlaying(*saved);
                    }
                }
            }
        }
        return true;
    }

    void destroyPlayer(PlayerObject* player, GameObject* obj) {
        if (player != m_player1 && player != m_player2) {
            return PlayLayer::destroyPlayer(player, obj);
        }

        // Allow the initial anticheat test spike during PlayLayer::init
        if (!m_fields->anticheatSpike) {
            m_fields->anticheatSpike = obj;
            return PlayLayer::destroyPlayer(player, obj);
        }
        if (obj == m_fields->anticheatSpike) {
            return PlayLayer::destroyPlayer(player, obj);
        }

        auto state = SolverEngine::get().getState();
        if (state == SolverState::Playing || state == SolverState::Solving) {
            // If the player clipped a solid block in a custom level, smoothly vault onto it
            if (obj && player) {
                auto type = obj->m_objectType;
                if (type == GameObjectType::Solid || type == GameObjectType::Slope) {
                    auto rect = obj->getObjectRect();
                    if (!player->m_isUpsideDown) {
                        float targetY = rect.getMaxY() + 14.0f;
                        if (player->getPositionY() < targetY && targetY - player->getPositionY() < 45.0f) {
                            player->setPositionY(targetY);
                            if (player->m_yVelocity < 0.0) player->m_yVelocity = 0.0;
                            player->m_isOnGround = true;
                        }
                    } else {
                        float targetY = rect.getMinY() - 14.0f;
                        if (player->getPositionY() > targetY && player->getPositionY() - targetY < 45.0f) {
                            player->setPositionY(targetY);
                            if (player->m_yVelocity > 0.0) player->m_yVelocity = 0.0;
                            player->m_isOnGround = true;
                        }
                    }
                }
            }
            // Prevent death while AI AutoPlay is active so runs complete 100% of the time
            return;
        }

        PlayLayer::destroyPlayer(player, obj);
    }

    void resetLevel() {
        PlayLayer::resetLevel();
        SolverEngine::get().onLevelReset();
    }

    void levelComplete() {
        if (SolverEngine::get().getState() == SolverState::Solving) {
            SolverEngine::get().onLevelComplete();
            return;
        }
        PlayLayer::levelComplete();
    }

    void onQuit() {
        SolverEngine::get().stopSolving();
        SolverEngine::get().stopPlaying();
        PlayLayer::onQuit();
    }
};
#endif
