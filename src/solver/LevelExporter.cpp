#include "LevelExporter.hpp"

#if __has_include(<Geode/Geode.hpp>)
#include <Geode/Geode.hpp>
#include <Geode/modify/GJBaseGameLayer.hpp>

using namespace geode::prelude;

namespace {

inline float forceOf(GameObject* obj) {
    auto* fb = geode::cast::typeinfo_cast<ForceBlockGameObject*>(obj);
    return fb ? fb->m_force : 0.f;
}

// Byte offset for m_isPassable on Windows 64-bit GD 2.2081
constexpr std::size_t kObjPassable = 0x515;

} // namespace

namespace LevelExporter {

void writeObjRects(std::ostream& rf, GJBaseGameLayer* l) {
    if (!l || !l->m_objects) return;

    rf << "id,type,cx,cy,w,h,groups,uid,radius,rot,sy0,sy1,shz,sdir,sup,w0,h0,"
          "tpy,tpg,tpix,tpiy,tw,zoom,zdur,zease,zrate,mvdir,gnddir,"
          "optp1,optp2,flipx,flipy,nofx,notouch,tpex,tpey,dis,"
          "editvel,vmodx,vmody,ovrvel,force,free,touch,spawn,chan,axis,exstat,rev,"
          "nocol,multiact,nomultiact,gmod,gpl,dontslide,"
          "tpfm,tpf,tpang,tprmod,tprmin,tprmax,"
          "optudg,"
          "extcol\n";

    PlayLayer* const pl = geode::cast::typeinfo_cast<PlayLayer*>(l);
    GameObject* const anticheat = pl ? pl->m_anticheatSpike : nullptr;

    for (auto* obj : CCArrayExt<GameObject*>(l->m_objects)) {
        if (!obj || obj == anticheat) continue;

        auto r = obj->getObjectRect();
        double sy0 = 0.0, sy1 = 0.0;
        int shz = 0, sdir = 0, sup = 0;
        if ((int)obj->m_objectType == 25) {
            sy0 = obj->slopeYPos(r.origin.x);
            sy1 = obj->slopeYPos(r.origin.x + r.size.width);
            shz = obj->m_slopeIsHazard ? 1 : 0;
            sdir = obj->m_slopeDirection;
            sup = obj->m_slopeUphill ? 1 : 0;
        }

        double tpy = 0.0, tpex = 0.0, tpey = 0.0;
        int tpg = 0, tpix = 0, tpiy = 0;
        int tpfm = 0;
        float tpf = 0.f, tpang = 0.f, tprmod = 0.f, tprmin = 0.f, tprmax = 0.f;

        if ((int)obj->m_objectType == 28 || obj->m_objectID == 3022
            || ((int)obj->m_objectType == 46
                && geode::cast::typeinfo_cast<TeleportPortalObject*>(obj))) {
            auto* tp = static_cast<TeleportPortalObject*>(obj);
            tpy = obj->getRealPosition().y + tp->m_teleportYOffset;
            tpg = tp->m_gravityMode;
            tpix = tp->m_ignoreX ? 1 : 0;
            tpiy = tp->m_ignoreY ? 1 : 0;
            GameObject* exitObj = nullptr;
            if (tp->m_orangePortal) {
                exitObj = tp->m_orangePortal;
                auto ep = tp->m_orangePortal->getRealPosition();
                tpex = ep.x;
                tpey = ep.y;
            } else if (tp->m_targetGroupID > 0) {
                auto* grp = l->getGroup(tp->m_targetGroupID);
                const int n = grp ? (int)grp->count() : 0;
                if (n >= 1) {
                    auto* ex = static_cast<GameObject*>(grp->objectAtIndex(0));
                    exitObj = ex;
                    auto ep = ex->getRealPosition();
                    tpex = ep.x;
                    tpey = ep.y;
                }
            }

            tpfm = (tp->m_staticForceEnabled ? 1 : 0) | (tp->m_staticForceAdditive ? 2 : 0)
                   | (tp->m_redirectForceEnabled ? 4 : 0);
            tpf = tp->m_staticForce;
            tprmod = tp->m_redirectForceMod;
            tprmin = tp->m_redirectForceMin;
            tprmax = tp->m_redirectForceMax;
            if (exitObj) {
                const int eid = exitObj->m_objectID;
                const bool portalLike = eid == 38 || eid == 747 || eid == 749 || eid == 2064
                                        || eid == 2902;
                const float a = (portalLike ? 180.f : 90.f) - exitObj->getRotation();
                tpang = (exitObj->m_isFlipX ? 180.f : 0.f) + a;
            } else {
                tpang = (obj->m_isFlipX ? 180.f : 0.f) - obj->getRotation();
            }
        }

        double tw = 0.0;
        if (obj->m_objectID == 1935)
            tw = static_cast<EffectGameObject*>(obj)->m_timeWarpTimeMod;

        double zm = 0.0, zdur = 0.0, zrate = 0.0;
        int zease = 0;
        if (obj->m_objectID == 1913) {
            auto* e = static_cast<EffectGameObject*>(obj);
            zm = e->m_zoomValue;
            zdur = e->m_duration;
            zease = (int)e->m_easingType;
            zrate = e->m_easingRate;
        }

        int mvdir = 0, gnddir = 0;
        int editvel = 0, ovrvel = 0, dontslide = 0;
        float vmodx = 0.f, vmody = 0.f;
        if (obj->m_objectID == 2900) {
            auto* rg = static_cast<RotateGameplayGameObject*>(obj);
            mvdir = rg->m_moveDirection;
            gnddir = rg->m_groundDirection;
            editvel = rg->m_editVelocity ? 1 : 0;
            ovrvel = rg->m_overrideVelocity ? 1 : 0;
            dontslide = rg->m_dontSlide ? 1 : 0;
            vmodx = rg->m_velocityModX;
            vmody = rg->m_velocityModY;
        }

        const int flx = obj->m_isFlipX ? 1 : 0;
        const int fly = obj->m_isFlipY ? 1 : 0;
        const int nofx = obj->m_hasNoEffects ? 1 : 0;
        const int notch = obj->m_isNoTouch ? 1 : 0;
        int optp1 = 0, optp2 = 0, optudg = 0;
        if (obj->m_objectID == 2899) {
            auto* go = static_cast<GameOptionsTrigger*>(obj);
            optp1 = (int)go->m_disableP1Controls;
            optp2 = (int)go->m_disableP2Controls;
            optudg = (int)go->m_unlinkDualGravity;
        }

        int touch = 0, spawn = 0, chan = 0, axis = 0, exstat = 0, freem = 0, rev = 0;
        if (auto* e = geode::cast::typeinfo_cast<EffectGameObject*>(obj)) {
            rev = e->m_isReverse ? 1 : 0;
            touch = e->m_isTouchTriggered ? 1 : 0;
            spawn = e->m_isSpawnTriggered ? 1 : 0;
            chan = e->m_channelValue;
            axis = (int)e->m_moveTargetMode;
            freem = e->m_cameraIsFreeMode ? 1 : 0;
        }
        if (auto* c = geode::cast::typeinfo_cast<CameraTriggerGameObject*>(obj))
            exstat = c->m_exitStatic ? 1 : 0;
        int multiAct = 0, noMultiAct = 0;
        if (auto* eg = geode::cast::typeinfo_cast<EnhancedGameObject*>(obj)) {
            multiAct = eg->m_isMultiActivate ? 1 : 0;
            noMultiAct = eg->m_isNoMultiActivate ? 1 : 0;
        }
        double gmod = 0.0;
        int gpl = 0;
        if (obj->m_objectID == 2066) {
            auto* e = static_cast<EffectGameObject*>(obj);
            gmod = e->m_gravityValue;
            gpl = (e->m_targetPlayer1 ? 1 : 0) | (e->m_targetPlayer2 ? 2 : 0)
                  | (e->m_followCPP ? 4 : 0);
        }

        rf << obj->m_objectID << "," << (int)obj->m_objectType << ","
           << (r.origin.x + r.size.width * 0.5f) << ","
           << (r.origin.y + r.size.height * 0.5f) << ","
           << r.size.width << "," << r.size.height << ","
           << (int)obj->m_groupCount << "," << obj->m_uniqueID << ","
           << obj->m_objectRadius << "," << obj->getRotation() << ","
           << sy0 << "," << sy1 << "," << shz << "," << sdir << "," << sup
           << "," << obj->m_width << "," << obj->m_height
           << "," << tpy << "," << tpg << "," << tpix << "," << tpiy
           << "," << tw
           << "," << zm << "," << zdur << "," << zease << "," << zrate
           << "," << mvdir << "," << gnddir
           << "," << optp1 << "," << optp2
           << "," << flx << "," << fly << "," << nofx << "," << notch
           << "," << tpex << "," << tpey
           << "," << (obj->m_isDisabled ? 1 : 0)
           << "," << editvel << "," << vmodx << "," << vmody << "," << ovrvel
           << "," << forceOf(obj)
           << "," << freem << "," << touch << "," << spawn << "," << chan
           << "," << axis << "," << exstat << "," << rev
           << "," << (int)(unsigned char)reinterpret_cast<const char*>(obj)[kObjPassable]
           << "," << multiAct << "," << noMultiAct
           << "," << gmod << "," << gpl
           << "," << dontslide
           << "," << tpfm << "," << tpf << "," << tpang
           << "," << tprmod << "," << tprmin << "," << tprmax
           << "," << optudg
           << "," << (obj->m_hasExtendedCollision ? 1 : 0)
           << "\n";
    }
}

std::string exportLevelCsv(GJBaseGameLayer* layer) {
    std::ostringstream ss;
    writeObjRects(ss, layer);
    return ss.str();
}

float getMaxLevelX(GJBaseGameLayer* layer) {
    if (!layer || !layer->m_objects) return 0.0f;
    float maxX = 0.0f;
    for (auto* obj : CCArrayExt<GameObject*>(layer->m_objects)) {
        if (!obj) continue;
        if (obj->getPositionX() > maxX) {
            maxX = obj->getPositionX();
        }
    }
    return maxX;
}

} // namespace LevelExporter
#else
namespace LevelExporter {
    void writeObjRects(std::ostream& rf, GJBaseGameLayer* layer) {}
    std::string exportLevelCsv(GJBaseGameLayer* layer) { return ""; }
    float getMaxLevelX(GJBaseGameLayer* layer) { return 0.0f; }
}
#endif
