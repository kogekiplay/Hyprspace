#include "Overview.hpp"
#include "Globals.hpp"

#include <algorithm>
#include <cctype>
#include <optional>

#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/config/shared/animation/AnimationTree.hpp>

namespace {

std::optional<Config::BOOL> oHyprbarsBarBlur;
int                         hyprbarsBlurSuppressors = 0;

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

Config::BOOL* hyprbarsBarBlurPtr() {
    static auto hyprbarsBarBlur = CConfigValue<Config::BOOL>("plugin:hyprbars:bar_blur");
    if (!hyprbarsBarBlur.good())
        return nullptr;

    return hyprbarsBarBlur.ptr();
}

std::string compactNamespaceToken(std::string token) {
    token.erase(std::remove_if(token.begin(), token.end(), [](unsigned char c) { return std::isspace(c); }), token.end());
    return token;
}

bool namespaceTokenMatches(const std::string& ns, std::string token) {
    token = compactNamespaceToken(std::move(token));
    if (token.empty())
        return false;

    if (token.ends_with('*'))
        return ns.starts_with(token.substr(0, token.size() - 1));

    return ns == token;
}

bool shouldKeepRealLayer(PHLLS layer) {
    if (!layer)
        return false;

    const auto& ns   = layer->m_namespace;
    const auto& list = Config::keepRealLayerNamespaces;

    size_t start = 0;
    while (start <= list.size()) {
        const auto end = list.find(',', start);
        if (namespaceTokenMatches(ns, list.substr(start, end == std::string::npos ? std::string::npos : end - start)))
            return true;

        if (end == std::string::npos)
            break;

        start = end + 1;
    }

    return false;
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

        layerAlpha(layer)->setValueAndWarp(alpha);
        *layerAlpha(layer) = alpha;
    }

    oLayerAlpha.clear();
}

void CHyprspaceWidget::hideRealLayers(PHLMONITOR owner) {
    if (!owner || !Config::hideRealLayers)
        return;

    for (int layerIdx : {2, 3}) {
        for (auto& layerRef : owner->m_layerSurfaceLayers[layerIdx]) {
            const auto layer = layerRef.lock();
            if (!layer || shouldKeepRealLayer(layer))
                continue;

            const bool alreadyHidden = std::ranges::any_of(oLayerAlpha, [&](const auto& hidden) {
                return std::get<0>(hidden) == layer;
            });
            if (!alreadyHidden)
                oLayerAlpha.emplace_back(layer, layerAlpha(layer)->goal());

            layerAlpha(layer)->setValueAndWarp(0.F);
            *layerAlpha(layer) = 0.F;
        }
    }
}

void CHyprspaceWidget::applyWindowNoBlur(PHLMONITOR owner) {
    if (!owner)
        return;

    for (const auto& window : windows()) {
        if (!window || !window->m_isMapped || !window->m_workspace || !window->m_workspace->m_monitor || window->m_workspace->m_monitor->m_id != ownerID)
            continue;

        if (window->m_ruleApplicator->noBlur().valueOrDefault())
            continue;

        const bool alreadyStored = std::ranges::any_of(oWindowNoBlur, [&](const auto& storedRef) {
            return storedRef.lock() == window;
        });
        if (alreadyStored)
            continue;

        oWindowNoBlur.emplace_back(window);
        window->m_ruleApplicator->noBlur().set(true, Desktop::Types::PRIORITY_SET_PROP);
    }
}

void CHyprspaceWidget::restoreWindowNoBlur() {
    for (const auto& windowRef : oWindowNoBlur) {
        const auto window = windowRef.lock();
        if (!window)
            continue;

        window->m_ruleApplicator->noBlur().unset(Desktop::Types::PRIORITY_SET_PROP);
    }

    oWindowNoBlur.clear();
}

void CHyprspaceWidget::disableOverviewBarBlur() {
    if (suppressingBarBlur)
        return;

    suppressingBarBlur = true;
    if (hyprbarsBlurSuppressors++ > 0)
        return;

    auto* barBlur = hyprbarsBarBlurPtr();
    if (!barBlur)
        return;

    oHyprbarsBarBlur = *barBlur;
    *barBlur = false;
}

void CHyprspaceWidget::restoreOverviewBarBlur() {
    if (!suppressingBarBlur)
        return;

    suppressingBarBlur = false;
    if (hyprbarsBlurSuppressors <= 0) {
        hyprbarsBlurSuppressors = 0;
        return;
    }

    if (--hyprbarsBlurSuppressors > 0)
        return;

    auto* barBlur = hyprbarsBarBlurPtr();
    if (barBlur && oHyprbarsBarBlur)
        *barBlur = *oHyprbarsBarBlur;

    oHyprbarsBarBlur.reset();
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
    restoreWindowNoBlur();
    restoreOverviewBarBlur();
    restoreFullscreenWindows();
    workspaceBoxes.clear();
    if (overviewDragActive && g_layoutManager->dragController()->target())
        g_layoutManager->endDragTarget();
    overviewDragActive = false;
    passingThroughActiveWindowDecoration = false;
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

    disableOverviewBarBlur();
    hideRealLayers(owner);
    applyWindowNoBlur(owner);

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
    restoreWindowNoBlur();
    restoreOverviewBarBlur();
    restoreFullscreenWindows();
    passingThroughActiveWindowDecoration = false;

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
