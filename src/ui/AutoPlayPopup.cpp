#include "AutoPlayPopup.hpp"
#include "../solver/SolverEngine.hpp"
#include "../macro/MacroManager.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/binding/PlayLayer.hpp>

using namespace geode::prelude;

AutoPlayPopup* AutoPlayPopup::create() {
    auto ret = new AutoPlayPopup();
    if (ret && ret->init()) {
        ret->autorelease();
        return ret;
    }
    CC_SAFE_DELETE(ret);
    return nullptr;
}

bool AutoPlayPopup::init() {
    if (!Popup::init(390.0f, 260.0f)) return false;

    this->setTitle("AI AutoPlay Hub");

    auto winSize = m_mainLayer->getContentSize();
    float centerX = winSize.width / 2.0f;

    auto playLayer = PlayLayer::get();
    int levelId = (playLayer && playLayer->m_level) ? playLayer->m_level->m_levelID : 0;
    std::string levelName = (playLayer && playLayer->m_level) ? playLayer->m_level->m_levelName : "Unknown";

    // Sleek midnight dark card for stats
    auto bgCard = CCScale9Sprite::create("square02_001.png");
    if (bgCard) {
        bgCard->setContentSize({354.0f, 102.0f});
        bgCard->setOpacity(170);
        bgCard->setColor({12, 16, 26});
        bgCard->setPosition({centerX, winSize.height - 90.0f});
        m_mainLayer->addChild(bgCard);
    }

    bool isSaved = MacroManager::get().hasSavedMacro(levelId);
    auto currentState = SolverEngine::get().getState();

    // 1. Current Status Display Badge
    std::string statusText = "[ STATUS: READY ]";
    ccColor3B statusColor = {220, 220, 220};
    if (currentState == SolverState::Solving) {
        statusText = "[ HEADLESS SOLVER ACTIVE ]";
        statusColor = {60, 240, 255}; // Radiant Cyan
    } else if (currentState == SolverState::Playing) {
        statusText = "[ AUTO-PLAYING 100% MACRO ]";
        statusColor = {50, 255, 120}; // Emerald Green
    } else if (isSaved) {
        statusText = "[ 100% SOLVED & VERIFIED ]";
        statusColor = {60, 255, 140};
    }

    m_statusLabel = CCLabelBMFont::create(statusText.c_str(), "bigFont.fnt");
    m_statusLabel->setScale(0.44f);
    m_statusLabel->setColor(statusColor);
    m_statusLabel->setPosition({centerX, winSize.height - 62.0f});
    m_mainLayer->addChild(m_statusLabel);

    // 2. Level Info
    std::string attemptsText = "Level: " + levelName + "  (ID: " + std::to_string(levelId) + ")";
    m_attemptsLabel = CCLabelBMFont::create(attemptsText.c_str(), "goldFont.fnt");
    m_attemptsLabel->setScale(0.42f);
    m_attemptsLabel->setPosition({centerX, winSize.height - 88.0f});
    m_mainLayer->addChild(m_attemptsLabel);

    // 3. Saved Macro Details
    std::string savedText = isSaved ? "Macro: Ready for 1x Playback (Instant)" : "Macro: Not generated yet (Click Solve)";
    m_savedInfoLabel = CCLabelBMFont::create(savedText.c_str(), "chatFont.fnt");
    m_savedInfoLabel->setScale(0.70f);
    m_savedInfoLabel->setColor(isSaved ? ccColor3B{80, 255, 120} : ccColor3B{255, 190, 80});
    m_savedInfoLabel->setPosition({centerX, winSize.height - 116.0f});
    m_mainLayer->addChild(m_savedInfoLabel);

    // 4. Action Buttons Menu
    auto menu = CCMenu::create();
    menu->setPosition({centerX, 62.0f});

    // Row 1: Primary Actions
    auto solveBtnSprite = ButtonSprite::create("Solve Level", "goldFont.fnt", "GJ_button_01.png", 0.75f);
    auto solveBtn = CCMenuItemSpriteExtra::create(
        solveBtnSprite, this, menu_selector(AutoPlayPopup::onSolve)
    );
    solveBtn->setPosition({-85.0f, 22.0f});
    menu->addChild(solveBtn);

    auto playBtnSprite = ButtonSprite::create("Play Macro", "goldFont.fnt", "GJ_button_02.png", 0.75f);
    auto playBtn = CCMenuItemSpriteExtra::create(
        playBtnSprite, this, menu_selector(AutoPlayPopup::onPlay)
    );
    playBtn->setPosition({85.0f, 22.0f});
    menu->addChild(playBtn);

    // Row 2: Secondary / Control Actions
    auto stopBtnSprite = ButtonSprite::create("Stop", "goldFont.fnt", "GJ_button_06.png", 0.65f);
    auto stopBtn = CCMenuItemSpriteExtra::create(
        stopBtnSprite, this, menu_selector(AutoPlayPopup::onStop)
    );
    stopBtn->setPosition({-105.0f, -24.0f});
    menu->addChild(stopBtn);

    bool coinsEnabled = Mod::get()->getSettingValue<bool>("collect_coins");
    std::string coinText = coinsEnabled ? "Coins: ON" : "Coins: OFF";
    m_coinsBtnSprite = ButtonSprite::create(coinText.c_str(), "goldFont.fnt", coinsEnabled ? "GJ_button_02.png" : "GJ_button_04.png", 0.65f);
    auto coinsBtn = CCMenuItemSpriteExtra::create(
        m_coinsBtnSprite, this, menu_selector(AutoPlayPopup::onToggleCoins)
    );
    coinsBtn->setPosition({0.0f, -24.0f});
    menu->addChild(coinsBtn);

    auto deleteBtnSprite = ButtonSprite::create("Reset", "chatFont.fnt", "GJ_button_05.png", 0.65f);
    auto deleteBtn = CCMenuItemSpriteExtra::create(
        deleteBtnSprite, this, menu_selector(AutoPlayPopup::onDeleteSaved)
    );
    deleteBtn->setPosition({105.0f, -24.0f});
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

void AutoPlayPopup::onToggleCoins(CCObject*) {
    bool current = Mod::get()->getSettingValue<bool>("collect_coins");
    bool next = !current;
    Mod::get()->setSettingValue("collect_coins", next);

    std::string text = next ? "Coins: ON" : "Coins: OFF";
    if (m_coinsBtnSprite) {
        m_coinsBtnSprite->setString(text.c_str());
    }
    Notification::create(next ? "Secret Coins Route: ENABLED" : "Secret Coins Route: DISABLED", NotificationIcon::Info)->show();
}

void AutoPlayPopup::onPlay(CCObject*) {
    auto playLayer = PlayLayer::get();
    if (!playLayer || !playLayer->m_level) {
        Notification::create("Enter a level first!", NotificationIcon::Warning)->show();
        return;
    }

    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    auto macro = MacroManager::get().loadMacro(playLayer->m_level->m_levelID, collectCoins);
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

    bool collectCoins = Mod::get()->getSettingValue<bool>("collect_coins");
    if (MacroManager::get().deleteMacro(playLayer->m_level->m_levelID, collectCoins)) {
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
