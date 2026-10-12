#include "AutoPlayPopup.hpp"
#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

AutoPlayPopup* AutoPlayPopup::create() {
    auto ret = new AutoPlayPopup();
    if (ret && ret->initAnchored(380.0f, 250.0f)) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool AutoPlayPopup::setup() {
    this->setTitle("AI AutoPlay Hub");

    auto winSize = m_mainLayer->getContentSize();
    float centerX = winSize.width / 2.0f;

    auto playLayer = PlayLayer::get();
    int levelId = (playLayer && playLayer->m_level) ? playLayer->m_level->m_levelID : 0;
    std::string levelName = (playLayer && playLayer->m_level) ? playLayer->m_level->m_levelName : "Unknown";

    // 1. Current Status Display
    std::string statusText = "Status: Idle";
    auto currentState = SolverEngine::get().getState();
    if (currentState == SolverState::Solving) {
        statusText = "Status: Solving (Fast-Forward)";
    } else if (currentState == SolverState::Playing) {
        statusText = "Status: Auto-Playing Macro";
    }

    m_statusLabel = CCLabelBMFont::create(statusText.c_str(), "bigFont.fnt");
    m_statusLabel->setScale(0.45f);
    m_statusLabel->setPosition({centerX, winSize.height - 55.0f});
    m_mainLayer->addChild(m_statusLabel);

    // 2. Attempts / Level Info
    std::string attemptsText = "Attempts: " + std::to_string(SolverEngine::get().getTotalAttempts()) +
                               " | Level: " + levelName + " (" + std::to_string(levelId) + ")";
    m_attemptsLabel = CCLabelBMFont::create(attemptsText.c_str(), "goldFont.fnt");
    m_attemptsLabel->setScale(0.4f);
    m_attemptsLabel->setPosition({centerX, winSize.height - 80.0f});
    m_mainLayer->addChild(m_attemptsLabel);

    // 3. Saved Macro Status
    bool isSaved = MacroManager::get().hasSavedMacro(levelId);
    std::string savedText = isSaved ? "Winning Macro: SAVED ON DISK" : "Winning Macro: Not Solved Yet";
    m_savedInfoLabel = CCLabelBMFont::create(savedText.c_str(), "chatFont.fnt");
    m_savedInfoLabel->setScale(0.6f);
    m_savedInfoLabel->setColor(isSaved ? ccColor3B{80, 255, 80} : ccColor3B{255, 180, 80});
    m_savedInfoLabel->setPosition({centerX, winSize.height - 110.0f});
    m_mainLayer->addChild(m_savedInfoLabel);

    // 4. Action Buttons Menu
    auto menu = CCMenu::create();
    menu->setPosition({centerX, 70.0f});

    // Button: Solve Current Level
    auto solveBtnSprite = ButtonSprite::create("Solve Level", "goldFont.fnt", "GJ_button_01.png", 0.75f);
    auto solveBtn = CCMenuItemSpriteExtra::create(
        solveBtnSprite, this, menu_selector(AutoPlayPopup::onSolve)
    );
    solveBtn->setPosition({-100.0f, 25.0f});
    menu->addChild(solveBtn);

    // Button: Play Saved
    auto playBtnSprite = ButtonSprite::create("Play Saved", "goldFont.fnt", "GJ_button_02.png", 0.75f);
    auto playBtn = CCMenuItemSpriteExtra::create(
        playBtnSprite, this, menu_selector(AutoPlayPopup::onPlay)
    );
    playBtn->setPosition({100.0f, 25.0f});
    menu->addChild(playBtn);

    // Button: Stop All
    auto stopBtnSprite = ButtonSprite::create("Stop", "bigFont.fnt", "GJ_button_06.png", 0.65f);
    auto stopBtn = CCMenuItemSpriteExtra::create(
        stopBtnSprite, this, menu_selector(AutoPlayPopup::onStop)
    );
    stopBtn->setPosition({-60.0f, -25.0f});
    menu->addChild(stopBtn);

    // Button: Delete Macro
    auto deleteBtnSprite = ButtonSprite::create("Clear Macro", "bigFont.fnt", "GJ_button_05.png", 0.65f);
    auto deleteBtn = CCMenuItemSpriteExtra::create(
        deleteBtnSprite, this, menu_selector(AutoPlayPopup::onDeleteSaved)
    );
    deleteBtn->setPosition({60.0f, -25.0f});
    menu->addChild(deleteBtn);

    m_mainLayer->addChild(menu);
    return true;
}

void AutoPlayPopup::onSolve(CCObject*) {
    auto playLayer = PlayLayer::get();
    if (!playLayer || !playLayer->m_level) {
        Notification::create("Enter a level first!", NotificationIcon::Warning)->show();
        return;
    }

    SolverEngine::get().startSolving(playLayer->m_level->m_levelID, playLayer->m_level->m_levelName);
    this->onClose(nullptr);
}

void AutoPlayPopup::onPlay(CCObject*) {
    auto playLayer = PlayLayer::get();
    if (!playLayer || !playLayer->m_level) {
        Notification::create("Enter a level first!", NotificationIcon::Warning)->show();
        return;
    }

    auto macro = MacroManager::get().loadMacro(playLayer->m_level->m_levelID);
    if (!macro.has_value()) {
        Notification::create("No saved macro for this level! Click 'Solve Level' first.", NotificationIcon::Warning)->show();
        return;
    }

    SolverEngine::get().startPlaying(*macro);
    playLayer->resetLevel();
    this->onClose(nullptr);
}

void AutoPlayPopup::onStop(CCObject*) {
    SolverEngine::get().stopSolving();
    SolverEngine::get().stopPlaying();
    Notification::create("AI AutoPlay Stopped", NotificationIcon::Info)->show();
    this->onClose(nullptr);
}

void AutoPlayPopup::onDeleteSaved(CCObject*) {
    auto playLayer = PlayLayer::get();
    if (!playLayer || !playLayer->m_level) return;

    if (MacroManager::get().deleteMacro(playLayer->m_level->m_levelID)) {
        Notification::create("Macro Deleted from Disk", NotificationIcon::Success)->show();
        if (m_savedInfoLabel) {
            m_savedInfoLabel->setString("Winning Macro: Not Solved Yet");
            m_savedInfoLabel->setColor(ccColor3B{255, 180, 80});
        }
    } else {
        Notification::create("No macro to delete", NotificationIcon::Info)->show();
    }
}
#endif
