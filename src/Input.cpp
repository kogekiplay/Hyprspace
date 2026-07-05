#include "Overview.hpp"
#include "Globals.hpp"

#include <algorithm>
#include <cmath>

#include <hyprland/src/desktop/view/Window.hpp>

namespace {

double panelTravel(PHLMONITOR owner) {
    if (!owner)
        return 0.;

    return (Config::panelHeight + Config::reservedArea) * owner->m_scale;
}

double swipeClosedOffset() {
    return -Config::swipeClosedPadding;
}

double swipeVisibleThreshold() {
    return Config::swipeThreshold;
}

PHLWINDOW activeWorkspaceDecorationAt(PHLMONITOR owner, Vector2D coords) {
    if (!owner || !owner->m_activeWorkspace)
        return nullptr;

    const auto& allWindows = windows();
    for (auto it = allWindows.rbegin(); it != allWindows.rend(); ++it) {
        const auto& window = *it;
        if (!window || !window->m_isMapped || window->m_workspace != owner->m_activeWorkspace)
            continue;

        const auto inputBox  = window->getWindowBoxUnified(Desktop::View::INPUT_EXTENTS);
        const auto windowBox = window->getWindowBoxUnified(Desktop::View::WINDOW_ONLY);
        if (inputBox.containsPoint(coords) && !windowBox.containsPoint(coords))
            return window;
    }

    return nullptr;
}

} // namespace

bool CHyprspaceWidget::buttonEvent(bool pressed, Vector2D coords) {
    const auto owner = getOwner();
    if (!owner)
        return true;

    if (pressed) {
        passingThroughActiveWindowDecoration = activeWorkspaceDecorationAt(owner, coords) != nullptr;
        if (passingThroughActiveWindowDecoration)
            return true;
    } else if (passingThroughActiveWindowDecoration) {
        passingThroughActiveWindowDecoration = false;
        return true;
    }

    bool couldExit = false;
    if (pressed) {
        lastPressedTime = std::chrono::high_resolution_clock::now();
    } else if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - lastPressedTime).count() < Config::clickReleaseThresholdMs) {
        couldExit = true;
    }

    WORKSPACEID targetWorkspaceID = SPECIAL_WORKSPACE_START - 1;

    for (const auto& [workspaceID, workspaceBox] : workspaceBoxes) {
        if (!workspaceBox.containsPoint(coords))
            continue;

        targetWorkspaceID = workspaceID;
        break;
    }

    auto targetWorkspace = workspaceByID(targetWorkspaceID);
    if (!targetWorkspace && targetWorkspaceID >= SPECIAL_WORKSPACE_START)
        targetWorkspace = createWorkspace(targetWorkspaceID, owner->m_id);

    if (pressed) {
        if (g_layoutManager->dragController()->target())
            g_layoutManager->endDragTarget();

        overviewDragActive = false;

        if (Config::autoDrag && targetWorkspace == nullptr) {
            const auto window = windowAt(coords);
            if (window) {
                const auto target = window->layoutTarget();
                if (target) {
                    g_layoutManager->beginDragTarget(target, MBIND_MOVE);
                    overviewDragActive = true;
                }
            }
        }
    }

    const auto dragTarget             = g_layoutManager->dragController()->target();
    const auto targetWindow           = (!pressed && overviewDragActive && dragTarget) ? dragTarget->window() : nullptr;
    const bool shouldEndOverviewDrag  = !pressed && overviewDragActive;
    const bool shouldDropOverviewDrag = overviewDragActive && targetWindow && targetWorkspace != nullptr && !pressed;

    if (shouldDropOverviewDrag) {
        Desktop::globalWindowController()->moveWindowToWorkspace(targetWindow, targetWorkspace);
        if (targetWindow->m_isFloating) {
            const auto targetPos = owner->m_position + (owner->m_size / 2.) - (targetWindow->m_reportedSize / 2.);
            targetWindow->move(targetPos);
            targetWindow->m_realPosition->setValueAndWarp(targetPos);
        }

        if (Config::switchOnDrop) {
            monitorFromID(targetWorkspace->m_monitor->m_id)->changeWorkspace(targetWorkspace->m_id);
            if (Config::exitOnSwitch && active)
                hide();
        }

        updateLayout();
    } else if (targetWorkspace && !pressed) {
        if (targetWorkspace->m_isSpecialWorkspace) {
            owner->activeSpecialWorkspaceID() == targetWorkspaceID ? owner->setSpecialWorkspace(nullptr) : owner->setSpecialWorkspace(targetWorkspaceID);
        } else {
            monitorFromID(targetWorkspace->m_monitor->m_id)->changeWorkspace(targetWorkspace->m_id);
        }

        if (Config::exitOnSwitch && active)
            hide();
    } else if (Config::exitOnClick && targetWorkspace == nullptr && active && couldExit && !pressed) {
        hide();
    }

    if (shouldEndOverviewDrag) {
        if (g_layoutManager->dragController()->target())
            g_layoutManager->endDragTarget();

        overviewDragActive = false;
    }

    // While overview is active all left-click input is consumed by the panel.
    return false;
}

bool CHyprspaceWidget::axisEvent(double delta, wl_pointer_axis axis, Vector2D coords) {
    const auto owner = getOwner();
    if (!owner)
        return true;

    const auto travel = panelTravel(owner);
    CBox panelBox     = {owner->m_position.x, owner->m_position.y - curYOffset->value(), owner->m_transformedSize.x, travel};
    if (Config::onBottom)
        panelBox = {owner->m_position.x, owner->m_position.y + owner->m_transformedSize.y - travel + curYOffset->value(), owner->m_transformedSize.x, travel};

    if (panelBox.containsPoint(coords * owner->m_scale)) {
        if (axis == WL_POINTER_AXIS_HORIZONTAL_SCROLL)
            *workspaceScrollOffset = workspaceScrollOffset->goal() - delta * Config::workspaceScrollSpeed;
    } else if (Config::autoScroll && axis == WL_POINTER_AXIS_VERTICAL_SCROLL) {
        const auto relative = delta < 0 ? "r-1" : "r+1";
        const auto wsIDName = getWorkspaceIDNameFromString(relative);
        if (!workspaceByID(wsIDName.id))
            (void)createWorkspace(wsIDName.id, ownerID);

        owner->changeWorkspace(wsIDName.id);
    }

    // Keep wheel events scoped to overview while it is active.
    return false;
}

bool CHyprspaceWidget::isSwiping() {
    return swiping;
}

bool CHyprspaceWidget::beginSwipe(IPointer::SSwipeBeginEvent e) {
    (void)e;
    swiping           = true;
    activeBeforeSwipe = active;
    avgSwipeSpeed     = 0.;
    swipePoints       = 0;
    return false;
}

bool CHyprspaceWidget::updateSwipe(IPointer::SSwipeUpdateEvent e) {
    const auto absY          = std::abs(e.delta.y);
    const bool verticalSwipe = absY > 0.0 && (std::abs(e.delta.x) / absY) < 1.0;

    if (verticalSwipe) {
        if (swiping && e.fingers == static_cast<uint32_t>(Config::swipeFingers)) {
            const auto owner = getOwner();
            if (!owner)
                return true;

            const auto distance       = std::max(Config::swipeDistance, 1);
            const auto currentScaling = owner->m_size.x / static_cast<float>(distance);
            const auto travel         = panelTravel(owner);

            const double scrollDelta = e.delta.y * (Config::reverseSwipe ? -1 : 1) * (Config::onBottom ? -1 : 1) * currentScaling;

            curSwipeOffset += scrollDelta;
            curSwipeOffset = std::clamp<double>(curSwipeOffset, swipeClosedOffset(), travel);

            avgSwipeSpeed = (avgSwipeSpeed * swipePoints + scrollDelta) / (swipePoints + 1);
            swipePoints++;

            curYOffset->setValueAndWarp(travel - curSwipeOffset);

            if (curSwipeOffset < swipeVisibleThreshold() && active)
                hide();
            else if (curSwipeOffset > swipeVisibleThreshold() && !active)
                show();

            return false;
        }
    } else if (e.fingers == static_cast<uint32_t>(Config::swipeFingers) && active) {
        const auto owner = getOwner();
        if (!owner)
            return true;

        const auto travel = panelTravel(owner);
        CBox panelBox     = {owner->m_position.x, owner->m_position.y - curYOffset->value(), owner->m_transformedSize.x, travel};
        if (Config::onBottom)
            panelBox = {owner->m_position.x, owner->m_position.y + owner->m_transformedSize.y - travel + curYOffset->value(), owner->m_transformedSize.x, travel};

        if (panelBox.containsPoint(g_pInputManager->getMouseCoordsInternal() * owner->m_scale)) {
            workspaceScrollOffset->setValueAndWarp(workspaceScrollOffset->goal() + e.delta.x * Config::workspaceScrollSpeed);
            return false;
        }
    }

    return true;
}

bool CHyprspaceWidget::endSwipe(IPointer::SSwipeEndEvent e) {
    swiping = false;

    const auto owner  = getOwner();
    const auto travel = panelTravel(owner);

    if (e.cancelled) {
        if (active)
            hide();
        curSwipeOffset = swipeClosedOffset();
    } else if (activeBeforeSwipe) {
        if ((curSwipeOffset < travel * Config::swipeCancelRatio) || avgSwipeSpeed < -Config::swipeForceSpeed) {
            if (active)
                hide();
            else {
                *curYOffset    = travel;
                curSwipeOffset = swipeClosedOffset();
            }
        } else if (!active) {
            show();
        } else {
            *curYOffset    = 0;
            curSwipeOffset = travel;
        }
    } else {
        if ((curSwipeOffset > travel * (1.F - Config::swipeCancelRatio)) || avgSwipeSpeed > Config::swipeForceSpeed) {
            if (!active)
                show();
            else {
                *curYOffset    = 0;
                curSwipeOffset = travel;
            }
        } else if (active) {
            hide();
        } else {
            *curYOffset    = travel;
            curSwipeOffset = swipeClosedOffset();
        }
    }

    avgSwipeSpeed = 0.;
    swipePoints   = 0;
    return false;
}
