#include "Overview.hpp"
#include "Globals.hpp"
#include <algorithm>
#include <climits>
#include <limits>
#include <unordered_map>

#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/helpers/memory/Memory.hpp>
#include <hyprland/src/protocols/core/Compositor.hpp>
#include <hyprland/src/protocols/types/SurfaceState.hpp>
#include <hyprland/src/render/pass/BorderPassElement.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/render/pass/RendererHintsPassElement.hpp>
#include <hyprland/src/render/pass/SurfacePassElement.hpp>
#include <hyprutils/utils/ScopeGuard.hpp>

namespace {

struct SWorkspaceWindows {
    std::vector<PHLWINDOW> tiled;
    std::vector<PHLWINDOW> floating;
};

void renderRect(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectData;
    rectData.color = color;
    rectData.box   = box;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectData));
}

void renderRectWithBlur(CBox box, CHyprColor color) {
    CRectPassElement::SRectData rectData;
    rectData.color = color;
    rectData.box   = box;
    rectData.blur  = true;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(rectData));
}

void renderBorder(CBox box, const Config::CGradientValueData& gradient, int size) {
    CBorderPassElement::SBorderData borderData;
    borderData.box        = box;
    borderData.grad1      = gradient;
    borderData.round      = 0;
    borderData.a          = 1.F;
    borderData.borderSize = size;
    g_pHyprRenderer->m_renderPass.add(makeUnique<CBorderPassElement>(borderData));
}

void refreshWindowPreviewSurfaces(PHLWINDOW window, PHLMONITOR monitor, const Time::steady_tp& time) {
    if (!window || !monitor || !window->wlSurface() || !window->wlSurface()->resource())
        return;

    if (g_pHyprRenderer->shouldRenderWindow(window, monitor))
        return;

    window->wlSurface()->resource()->breadthfirst(
        [monitor, &time](SP<CWLSurfaceResource> surface, const Vector2D&, void*) {
            if (!surface)
                return;

            surface->m_stateQueue.unlockFirst(LOCK_REASON_FENCE | LOCK_REASON_FIFO | LOCK_REASON_TIMER);
            surface->presentFeedback(time, monitor, true);
        },
        nullptr);
}

void renderWindowStub(PHLWINDOW window, PHLMONITOR monitor, PHLWORKSPACE workspaceOverride, CBox rectOverride, CBox clipBox, const Time::steady_tp& time) {
    if (!window || !monitor || !workspaceOverride)
        return;

    if (!window->m_isMapped || !window->wlSurface() || !window->wlSurface()->resource())
        return;

    Render::SRenderModifData renderModif;

    const auto  workspace           = window->m_workspace;
    const auto  fullscreenState     = window->m_fullscreenState;
    const auto  realPosition        = window->m_realPosition->value();
    const auto  realSize            = window->m_realSize->value();
    const auto  pinned              = window->m_pinned;
    const auto  floating            = window->m_isFloating;
    const float logicalW            = std::max(static_cast<float>(realSize.x), 5.F);
    const float scaleMod            = rectOverride.w / std::max(logicalW * monitor->m_scale, 5.F);
    if (!(scaleMod > 0.F) || !(rectOverride.w > 0 && rectOverride.h > 0))
        return;

    refreshWindowPreviewSurfaces(window, monitor, time);

    const Vector2D logicalTL = realPosition + window->m_floatingOffset;
    const Vector2D scaledTL  = (logicalTL - monitor->m_position) * monitor->m_scale;
    const Vector2D translate = rectOverride.pos() / scaleMod - scaledTL;

    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(translate)));
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, std::any(scaleMod)));
    renderModif.enabled = true;

    window->m_workspace       = workspaceOverride;
    window->m_fullscreenState = Desktop::View::SFullscreenState{FSMODE_NONE};
    window->m_ruleApplicator->nearestNeighbor().set(false, Desktop::Types::PRIORITY_SET_PROP);
    window->m_isFloating = false;
    window->m_pinned     = true;
    window->m_ruleApplicator->noBlur().set(true, Desktop::Types::PRIORITY_SET_PROP);

    Hyprutils::Utils::CScopeGuard restoreWindowState([&] {
        window->m_workspace       = workspace;
        window->m_fullscreenState = fullscreenState;
        window->m_ruleApplicator->nearestNeighbor().unset(Desktop::Types::PRIORITY_SET_PROP);
        window->m_isFloating = floating;
        window->m_pinned     = pinned;
        window->m_ruleApplicator->noBlur().unset(Desktop::Types::PRIORITY_SET_PROP);
    });

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard clearHints([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = Render::SRenderModifData{}}));
    });

    g_pHyprRenderer->damageWindow(window);

    CSurfacePassElement::SRenderData renderData = {monitor, time};
    renderData.pos                = realPosition + window->m_floatingOffset;
    renderData.w                  = std::max(realSize.x, 5.0);
    renderData.h                  = std::max(realSize.y, 5.0);
    renderData.surface            = window->wlSurface()->resource();
    renderData.dontRound          = window->isEffectiveInternalFSMode(FSMODE_FULLSCREEN);
    renderData.fadeAlpha          = 1.F;
    renderData.alpha              = 1.F;
    renderData.decorate           = false;
    renderData.rounding           = renderData.dontRound ? 0 : window->rounding() * scaleMod * monitor->m_scale;
    renderData.roundingPower      = renderData.dontRound ? 2.F : window->roundingPower();
    renderData.blur               = false;
    renderData.pWindow            = window;
    renderData.clipBox            = clipBox;
    renderData.useNearestNeighbor = false;
    renderData.squishOversized    = true;
    renderData.surfaceCounter     = 0;

    window->wlSurface()->resource()->breadthfirst(
        [&renderData, &window](SP<CWLSurfaceResource> surface, const Vector2D& offset, void*) {
            if (!surface || !surface->m_current.texture)
                return;

            if (surface->m_current.size.x < 1 || surface->m_current.size.y < 1)
                return;

            renderData.localPos    = offset;
            renderData.texture     = surface->m_current.texture;
            renderData.surface     = surface;
            renderData.mainSurface = surface == window->wlSurface()->resource();
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(renderData));
            renderData.surfaceCounter++;
        },
        nullptr);
}

void renderLayerStub(PHLLS layer, PHLMONITOR monitor, CBox rectOverride, CBox clipBox, const Time::steady_tp& time) {
    if (!layer || !monitor)
        return;

    if (!layer->m_mapped || !layer->m_layerSurface || !layer->wlSurface() || !layer->wlSurface()->resource())
        return;

    const Vector2D realPosition = layer->m_realPosition->value();
    const Vector2D realSize     = layer->m_realSize->value();
    const float    alpha        = layerAlpha(layer)->value();

    const float scale = rectOverride.w / realSize.x;
    if (!(scale > 0.F) || !(rectOverride.w > 0 && rectOverride.h > 0))
        return;

    Render::SRenderModifData renderModif;
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_TRANSLATE, std::any(monitor->m_position + (rectOverride.pos() / scale) - realPosition)));
    renderModif.modifs.push_back(std::make_pair(Render::SRenderModifData::eRenderModifType::RMOD_TYPE_SCALE, std::any(scale)));
    renderModif.enabled = true;

    layerAlpha(layer)->setValueAndWarp(1.F);

    g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = renderModif}));
    Hyprutils::Utils::CScopeGuard clearHints([] {
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRendererHintsPassElement>(CRendererHintsPassElement::SData{.renderModif = Render::SRenderModifData{}}));
    });
    CSurfacePassElement::SRenderData renderData = {monitor, time, realPosition};
    renderData.fadeAlpha      = 1.F;
    renderData.alpha          = 1.F;
    renderData.blur           = false;
    renderData.surface        = layer->wlSurface()->resource();
    renderData.decorate       = false;
    renderData.w              = realSize.x;
    renderData.h              = realSize.y;
    renderData.pLS            = layer;
    renderData.clipBox        = clipBox;
    renderData.surfaceCounter = 0;

    layer->wlSurface()->resource()->breadthfirst(
        [&renderData, &layer](SP<CWLSurfaceResource> surface, const Vector2D& offset, void*) {
            if (!surface || !surface->m_current.texture)
                return;

            if (surface->m_current.size.x < 1 || surface->m_current.size.y < 1)
                return;

            renderData.localPos    = offset;
            renderData.texture     = surface->m_current.texture;
            renderData.surface     = surface;
            renderData.mainSurface = surface == layer->wlSurface()->resource();
            g_pHyprRenderer->m_renderPass.add(makeUnique<CSurfacePassElement>(renderData));
            renderData.surfaceCounter++;
        },
        nullptr);

    layerAlpha(layer)->setValueAndWarp(alpha);
}

bool renderWindowPreview(PHLWINDOW window, PHLWORKSPACE workspace, PHLMONITOR owner, CBox workspaceBox, double monitorScaleFactor, double previewCropTop, const Time::steady_tp& time) {
    if (!window || !workspace || !owner)
        return false;

    const double wX = workspaceBox.x + ((window->m_realPosition->value().x - owner->m_position.x) * monitorScaleFactor * owner->m_scale);
    const double wY = workspaceBox.y + ((window->m_realPosition->value().y - owner->m_position.y - previewCropTop) * monitorScaleFactor * owner->m_scale);
    const double wW = window->m_realSize->value().x * monitorScaleFactor * owner->m_scale;
    const double wH = window->m_realSize->value().y * monitorScaleFactor * owner->m_scale;
    if (!(wW > 0 && wH > 0))
        return false;

    renderWindowStub(window, owner, workspace, {wX, wY, wW, wH}, workspaceBox, time);
    return true;
}

void redrawActiveWorkspaceWindow(PHLWINDOW window, PHLWORKSPACE workspace, PHLMONITOR owner, CBox clipBox, const Time::steady_tp& time) {
    if (!window || !workspace || !owner)
        return;

    if (!pRenderWindow)
        return;

    const auto clipBefore = g_pHyprRenderer->m_renderData.clipBox;
    g_pHyprRenderer->m_renderData.clipBox = clipBox;

    Hyprutils::Utils::CScopeGuard restoreRenderState([&] {
        g_pHyprRenderer->m_renderData.clipBox = clipBefore;
    });

    (*(tRenderWindow)pRenderWindow)(g_pHyprRenderer.get(), window, owner, time, true, Render::RENDER_PASS_MAIN, false, false);
}

void redrawActiveWorkspaceWindows(PHLMONITOR owner, CBox clipBox, const Time::steady_tp& time) {
    if (!owner || !owner->m_activeWorkspace)
        return;

    const auto activeWorkspace = owner->m_activeWorkspace;

    SWorkspaceWindows activeWindows;
    for (const auto& window : windows()) {
        if (!window || !window->m_isMapped || window->m_workspace != activeWorkspace)
            continue;

        if (window->m_isFloating)
            activeWindows.floating.push_back(window);
        else
            activeWindows.tiled.push_back(window);
    }

    for (const auto& window : activeWindows.tiled)
        redrawActiveWorkspaceWindow(window, activeWorkspace, owner, clipBox, time);

    const auto focused = activeWorkspace->getLastFocusedWindow();
    for (const auto& window : activeWindows.floating) {
        if (window == focused)
            continue;

        redrawActiveWorkspaceWindow(window, activeWorkspace, owner, clipBox, time);
    }

    if (focused && focused->m_isMapped && focused->m_isFloating)
        redrawActiveWorkspaceWindow(focused, activeWorkspace, owner, clipBox, time);
}

} // namespace

void CHyprspaceWidget::draw() {
    workspaceBoxes.clear();

    if (compositorUnsafe())
        return;

    if (!active && !curYOffset->isBeingAnimated())
        return;

    const auto owner = getOwner();
    if (!owner || !owner->m_enabled)
        return;

    if (active)
        hideRealLayers(owner);

    const CBox monitorClip = {{0, 0}, owner->m_transformedSize};
    const auto time        = Time::steadyNow();

    const int panelDirection = Config::onBottom ? -1 : 1;

    CBox panelBox = {owner->m_position.x,
                     owner->m_position.y + (Config::onBottom * (owner->m_transformedSize.y - ((Config::panelHeight + Config::reservedArea) * owner->m_scale))) - (panelDirection * curYOffset->value()),
                     owner->m_transformedSize.x,
                     (Config::panelHeight + Config::reservedArea) * owner->m_scale};

    panelBox.x -= owner->m_position.x;
    panelBox.y -= owner->m_position.y;

    g_pHyprRenderer->m_renderData.clipBox = monitorClip;

    if (active) {
        owner->m_blurFBShouldRender = true;

        if (!Config::disableBlur)
            renderRectWithBlur(monitorClip, Config::overviewBackgroundColor);
        else
            renderRect(monitorClip, Config::overviewBackgroundColor);

        redrawActiveWorkspaceWindows(owner, monitorClip, time);
    }

    g_pHyprRenderer->damageMonitor(owner);

    std::vector<WORKSPACEID> workspaces;

    if (Config::showSpecialWorkspace)
        workspaces.push_back(SPECIAL_WORKSPACE_START);

    WORKSPACEID lowestID  = std::numeric_limits<WORKSPACEID>::max();
    WORKSPACEID highestID = 1;
    for (const auto& ws : workspaceList()) {
        if (!ws || ws->m_id < 1 || !ws->m_monitor || ws->m_monitor->m_id != ownerID)
            continue;

        workspaces.push_back(ws->m_id);
        highestID = std::max(highestID, ws->m_id);
        lowestID  = std::min(lowestID, ws->m_id);
    }

    if (Config::showEmptyWorkspace) {
        WORKSPACEID wsIDStart = 1;
        WORKSPACEID wsIDEnd   = highestID;

        if (numWorkspaces > 0) {
            const auto baseWorkspace = static_cast<WORKSPACEID>(numWorkspaces * ownerID + 1);
            wsIDStart                = std::min<WORKSPACEID>(baseWorkspace, lowestID);
            wsIDEnd                  = std::max<WORKSPACEID>(baseWorkspace, highestID);
        }

        for (WORKSPACEID id = wsIDStart; id <= wsIDEnd; id++) {
            if (id == owner->activeSpecialWorkspaceID())
                continue;

            if (workspaceByID(id) == nullptr)
                workspaces.push_back(id);
        }
    }

    if (Config::showNewWorkspace) {
        while (workspaceByID(highestID) != nullptr)
            highestID++;
        workspaces.push_back(highestID);
    }

    std::sort(workspaces.begin(), workspaces.end());
    workspaces.erase(std::unique(workspaces.begin(), workspaces.end()), workspaces.end());

    const auto workspaceCount = static_cast<int>(workspaces.size());
    if (workspaceCount == 0)
        return;

    const double previewCropTop     = std::clamp<double>(Config::workspacePreviewCropTop, 0., std::max(owner->m_transformedSize.y - 1., 0.));
    const double previewHeight      = std::max(owner->m_transformedSize.y - previewCropTop, 1.);
    const double previewPanelHeight = std::max<double>(Config::panelHeight - 2 * Config::workspaceMargin, 1.);
    const double monitorScaleFactor = (previewPanelHeight / previewHeight) * owner->m_scale;
    const double workspaceBoxW      = owner->m_transformedSize.x * monitorScaleFactor;
    const double workspaceBoxH      = previewHeight * monitorScaleFactor;
    const double workspaceGroupW    = workspaceBoxW * workspaceCount + (Config::workspaceMargin * owner->m_scale) * (workspaceCount - 1);
    double       workspaceOffsetX   = Config::centerAligned ? workspaceScrollOffset->value() + (panelBox.w / 2.) - (workspaceGroupW / 2.) : workspaceScrollOffset->value() + Config::workspaceMargin;
    const double workspaceOffsetY   = !Config::onBottom ? (((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - curYOffset->value()) : (owner->m_transformedSize.y - ((Config::reservedArea + Config::workspaceMargin) * owner->m_scale) - workspaceBoxH + curYOffset->value());
    const double workspaceOverflow  = std::max<double>(((workspaceGroupW - panelBox.w) / 2.) + (Config::workspaceMargin * owner->m_scale), 0.);

    *workspaceScrollOffset = std::clamp<double>(workspaceScrollOffset->goal(), -workspaceOverflow, workspaceOverflow);

    if (!(workspaceBoxW > 0 && workspaceBoxH > 0))
        return;

    const Vector2D previewOrigin = owner->m_position + Vector2D{0.0, previewCropTop};

    std::unordered_map<WORKSPACEID, SWorkspaceWindows> windowsByWorkspace;
    windowsByWorkspace.reserve(workspaceCount + 2);
    for (const auto& window : windows()) {
        if (!window || !window->m_workspace)
            continue;

        auto& bucket = windowsByWorkspace[window->m_workspace->m_id];
        if (window->m_isFloating)
            bucket.floating.push_back(window);
        else
            bucket.tiled.push_back(window);
    }

    for (const auto wsID : workspaces) {
        const auto ws  = workspaceByID(wsID);
        CBox       box = {workspaceOffsetX, workspaceOffsetY, workspaceBoxW, workspaceBoxH};

        if (ws == owner->m_activeWorkspace) {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceActiveBorder.a > 0)
                renderBorder(box, Config::CGradientValueData(Config::workspaceActiveBorder), Config::workspaceBorderSize);

            if (!Config::disableBlur)
                renderRectWithBlur(box, Config::workspaceActiveBackground);
            else
                renderRect(box, Config::workspaceActiveBackground);

            if (!Config::drawActiveWorkspace) {
                workspaceOffsetX += workspaceBoxW + (Config::workspaceMargin * owner->m_scale);
                continue;
            }
        } else {
            if (Config::workspaceBorderSize >= 1 && Config::workspaceInactiveBorder.a > 0)
                renderBorder(box, Config::CGradientValueData(Config::workspaceInactiveBorder), Config::workspaceBorderSize);

            if (!Config::disableBlur)
                renderRectWithBlur(box, Config::workspaceInactiveBackground);
            else
                renderRect(box, Config::workspaceInactiveBackground);
        }

        if (!Config::hideBackgroundLayers) {
            for (const auto& layerRef : owner->m_layerSurfaceLayers[0]) {
                const auto layer = layerRef.lock();
                if (!layer)
                    continue;

                CBox layerBox = {box.pos() + (layer->m_realPosition->value() - previewOrigin) * monitorScaleFactor, layer->m_realSize->value() * monitorScaleFactor};
                renderLayerStub(layer, owner, layerBox, box, time);
            }

            for (const auto& layerRef : owner->m_layerSurfaceLayers[1]) {
                const auto layer = layerRef.lock();
                if (!layer)
                    continue;

                CBox layerBox = {box.pos() + (layer->m_realPosition->value() - previewOrigin) * monitorScaleFactor, layer->m_realSize->value() * monitorScaleFactor};
                renderLayerStub(layer, owner, layerBox, box, time);
            }
        }

        if (owner->m_activeWorkspace == ws && Config::affectStrut && previewCropTop <= 0.) {
            CBox miniPanelBox = {workspaceOffsetX, workspaceOffsetY, panelBox.w * monitorScaleFactor, panelBox.h * monitorScaleFactor};
            if (Config::onBottom)
                miniPanelBox = {workspaceOffsetX, workspaceOffsetY + workspaceBoxH - panelBox.h * monitorScaleFactor, panelBox.w * monitorScaleFactor, panelBox.h * monitorScaleFactor};

            if (!Config::disableBlur)
                renderRectWithBlur(miniPanelBox, CHyprColor(0, 0, 0, 0));
            else
                renderRect(miniPanelBox, CHyprColor(0, 0, 0, 0));
        }

        if (ws) {
            const auto windowsIt = windowsByWorkspace.find(ws->m_id);
                if (windowsIt != windowsByWorkspace.end()) {
                    for (const auto& window : windowsIt->second.tiled)
                        renderWindowPreview(window, ws, owner, box, monitorScaleFactor, previewCropTop, time);

                const auto focused = ws->getLastFocusedWindow();
                for (const auto& window : windowsIt->second.floating) {
                    if (window == focused)
                        continue;

                    renderWindowPreview(window, ws, owner, box, monitorScaleFactor, previewCropTop, time);
                }

                if (focused && focused->m_isFloating)
                    renderWindowPreview(focused, ws, owner, box, monitorScaleFactor, previewCropTop, time);
            }
        }

        if (owner->m_activeWorkspace != ws || !Config::hideRealLayers) {
            if (!Config::hideTopLayers) {
                for (const auto& layerRef : owner->m_layerSurfaceLayers[2]) {
                    const auto layer = layerRef.lock();
                    if (!layer)
                        continue;

                    CBox layerBox = {box.pos() + (layer->m_realPosition->value() - previewOrigin) * monitorScaleFactor, layer->m_realSize->value() * monitorScaleFactor};
                    renderLayerStub(layer, owner, layerBox, box, time);
                }
            }

            if (!Config::hideOverlayLayers) {
                for (const auto& layerRef : owner->m_layerSurfaceLayers[3]) {
                    const auto layer = layerRef.lock();
                    if (!layer)
                        continue;

                    CBox layerBox = {box.pos() + (layer->m_realPosition->value() - previewOrigin) * monitorScaleFactor, layer->m_realSize->value() * monitorScaleFactor};
                    renderLayerStub(layer, owner, layerBox, box, time);
                }
            }
        }

        // Input hitboxes are evaluated in monitor-space logical coordinates.
        box.scale(1 / owner->m_scale);
        box.x += owner->m_position.x;
        box.y += owner->m_position.y;
        workspaceBoxes.emplace_back(wsID, box);

        workspaceOffsetX += workspaceBoxW + Config::workspaceMargin * owner->m_scale;
    }

    g_pHyprRenderer->m_renderData.clipBox = monitorClip;
}
