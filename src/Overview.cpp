#include "Overview.hpp"
#include "Globals.hpp"

#include <algorithm>

#include <hyprland/src/config/shared/animation/AnimationTree.hpp>

namespace {

double panelTravelForMonitor(PHLMONITOR owner) {
    if (!owner)
        return 0.;

    return (Config::panelHeight + Config::reservedArea) * owner->m_scale;
}

double closedSwipeOffset() {
    return -Config::swipeClosedPadding;
}

double shownSwipeOffset(PHLMONITOR owner) {
    return panelTravelForMonitor(owner);
}

void requestFullMonitorRedraw(PHLMONITOR owner) {
    if (!owner)
        return;

    owner->m_damage.damageEntire();
    scheduleFrameForMonitor(owner);
}

} // namespace

CHyprspaceWidget::CHyprspaceWidget(uint64_t inOwnerID) : ownerID(inOwnerID) {
    resetAnimationState(getOwner());
}

CHyprspaceWidget::~CHyprspaceWidget() {
    cleanup(getOwner());
    curYOffset.reset();
    workspaceScrollOffset.reset();
}

void CHyprspaceWidget::restoreHiddenLayers() {
    for (const auto& [layer, alpha] : oLayerAlpha) {
        if (!layer || !layer->m_mapped)
            continue;

        *layerAlpha(layer) = alpha;
    }

    oLayerAlpha.clear();
}

void CHyprspaceWidget::restoreFullscreenWindows() {
    for (const auto& [windowRef, fullscreenMode] : prevFullscreen) {
        const auto window = windowRef.lock();
        if (!window)
            continue;

        g_pCompositor->setWindowFullscreenState(window, Desktop::View::SFullscreenState{.internal = fullscreenMode, .client = fullscreenMode});
        if (fullscreenMode == FSMODE_FULLSCREEN)
            window->m_wantsInitialFullscreen = false;
    }

    prevFullscreen.clear();
}

void CHyprspaceWidget::resetAnimationState(PHLMONITOR owner) {
    curAnimationConfig = *Config::animationTree()->getAnimationPropertyConfig("windows");
    curAnimation       = *curAnimationConfig.pValues.lock();
    *curAnimationConfig.pValues.lock() = curAnimation;

    if (Config::overrideAnimSpeed > 0)
        curAnimation.internalSpeed = Config::overrideAnimSpeed;

    Animation::mgr()->createAnimation(0.F, curYOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);
    Animation::mgr()->createAnimation(0.F, workspaceScrollOffset, curAnimationConfig.pValues.lock(), AVARDAMAGE_ENTIRE);

    const auto hiddenOffset = panelTravelForMonitor(owner);
    curYOffset->setValueAndWarp(active ? 0.F : hiddenOffset);
    workspaceScrollOffset->setValueAndWarp(0.F);
    curSwipeOffset = active ? shownSwipeOffset(owner) : closedSwipeOffset();
}

void CHyprspaceWidget::cleanup(PHLMONITOR owner) {
    restoreHiddenLayers();
    restoreFullscreenWindows();
    workspaceBoxes.clear();
    if (overviewDragActive && g_layoutManager->dragController()->target())
        g_layoutManager->endDragTarget();
    overviewDragActive = false;
    swiping           = false;
    activeBeforeSwipe = false;
    avgSwipeSpeed     = 0.;
    swipePoints       = 0;
    active            = false;

    if (owner) {
        owner->m_reservedArea = Desktop::CReservedArea();
        g_pHyprRenderer->arrangeLayersForMonitor(ownerID);
        g_layoutManager->recalculateMonitor(owner);
        requestFullMonitorRedraw(owner);
    }
}

PHLMONITOR CHyprspaceWidget::getOwner() {
    return monitorFromID(ownerID);
}

void CHyprspaceWidget::show() {
    auto owner = getOwner();
    if (!owner || !owner->m_enabled || compositorUnsafe())
        return;

    if (prevFullscreen.empty()) {
        for (auto& ws : workspaceList()) {
            if (!ws || !ws->m_monitor || ws->m_monitor->m_id != ownerID)
                continue;

            const auto window = ws->getFullscreenWindow();
            if (!window || ws->m_fullscreenMode == FSMODE_NONE)
                continue;

            if (ws->m_fullscreenMode == FSMODE_FULLSCREEN)
                window->m_wantsInitialFullscreen = true;

            prevFullscreen.emplace_back(PHLWINDOWREF(window), ws->m_fullscreenMode);
            g_pCompositor->setWindowFullscreenState(window, Desktop::View::SFullscreenState{.internal = FSMODE_NONE, .client = FSMODE_NONE});
        }
    }

    if (oLayerAlpha.empty() && Config::hideRealLayers) {
        for (int layerIdx : {2, 3}) {
            for (auto& layerRef : owner->m_layerSurfaceLayers[layerIdx]) {
                const auto layer = layerRef.lock();
                if (!layer)
                    continue;

                oLayerAlpha.emplace_back(layer, layerAlpha(layer)->goal());
                *layerAlpha(layer) = 0.F;
            }
        }
    }

    active = true;

    if (!swiping) {
        *curYOffset    = 0.F;
        curSwipeOffset = shownSwipeOffset(owner);
    }

    updateLayout();
    g_pHyprRenderer->damageMonitor(owner);
    requestFullMonitorRedraw(owner);
}

void CHyprspaceWidget::hide() {
    auto owner = getOwner();
    if (!owner)
        return;

    restoreHiddenLayers();
    restoreFullscreenWindows();

    active = false;

    if (!swiping) {
        *curYOffset    = shownSwipeOffset(owner);
        curSwipeOffset = closedSwipeOffset();
    }

    updateLayout();
    requestFullMonitorRedraw(owner);
}

void CHyprspaceWidget::updateConfig() {
    resetAnimationState(getOwner());
}

bool CHyprspaceWidget::isActive() {
    return active;
}
