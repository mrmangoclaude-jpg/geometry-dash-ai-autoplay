#pragma once

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/ui/Popup.hpp>

class AutoPlayPopup : public geode::Popup {
protected:
    cocos2d::CCLabelBMFont* m_statusLabel = nullptr;
    cocos2d::CCLabelBMFont* m_attemptsLabel = nullptr;
    cocos2d::CCLabelBMFont* m_savedInfoLabel = nullptr;
    ButtonSprite* m_coinsBtnSprite = nullptr;

    bool init();

    void onSolve(cocos2d::CCObject* sender);
    void onPlay(cocos2d::CCObject* sender);
    void onStop(cocos2d::CCObject* sender);
    void onDeleteSaved(cocos2d::CCObject* sender);
    void onToggleCoins(cocos2d::CCObject* sender);

public:
    static AutoPlayPopup* create();
};

#else

class AutoPlayPopup {
public:
    static AutoPlayPopup* create() { return nullptr; }
    void show() {}
};

#endif
