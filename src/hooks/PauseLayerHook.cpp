#include "../ui/AutoPlayPopup.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/PauseLayer.hpp>

using namespace geode::prelude;

class $modify(AutoPlayPauseLayer, PauseLayer) {
    void customSetup() {
        PauseLayer::customSetup();

        auto rightMenu = this->getChildByID("right-button-menu");
        if (!rightMenu) {
            rightMenu = this->getChildByID("bottom-button-menu");
        }
        if (!rightMenu) return;

        auto sprite = CircleButtonSprite::createWithSpriteFrameName(
            "GJ_autoOnBtn_001.png", 0.95f, CircleBaseColor::Green
        );

        auto btn = CCMenuItemSpriteExtra::create(
            sprite, this, menu_selector(AutoPlayPauseLayer::onOpenAutoPlay)
        );
        btn->setID("ai-autoplay-btn"_spr);
        rightMenu->addChild(btn);
        rightMenu->updateLayout();
    }

    void onOpenAutoPlay(CCObject*) {
        auto popup = AutoPlayPopup::create();
        if (popup) {
            popup->show();
        }
    }
};
#endif
