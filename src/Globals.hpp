#pragma once

#include <functional>
#include <string>
#include <tuple>
#include <type_traits>

#include <hyprutils/memory/SharedPtr.hpp>

#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/types.hpp>
#include <hyprland/src/managers/input/InputManager.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/animation/AnimationManager.hpp>
#include <hyprland/src/config/ConfigValue.hpp>
#include <hyprland/src/desktop/state/GlobalWindowController.hpp>
#include <hyprland/src/desktop/state/WindowState.hpp>
#include <hyprland/src/desktop/view/LayerSurface.hpp>
#include <hyprland/src/pointer/PointerController.hpp>
#include <hyprland/src/state/MonitorQuery.hpp>
#include <hyprland/src/state/MonitorState.hpp>
#include <hyprland/src/state/WorkspaceQuery.hpp>
#include <hyprland/src/state/WorkspaceState.hpp>
#include <hyprland/src/helpers/time/Time.hpp>
#include <hyprland/src/event/EventBus.hpp>

// Hyprland v0.54+: cancellable input uses Event::SCallbackInfo (not legacy CEvent*).
using SCallbackInfo = Event::SCallbackInfo;

// Must match Hyprutils::Signal::CSignalT::RefArg (hyprutils/signal/Signal.hpp).
template <typename T>
using HyprSignalRefArg = std::conditional_t<std::is_trivially_copyable_v<T>, T, const T&>;

// Unpack Hyprutils::CSignalT::emit() tuple — first event arg is often stored by value (trivial types).
template <typename EventType, typename Signal>
CHyprSignalListener listenCancellable(Signal& signal, std::function<void(const EventType&, SCallbackInfo&)> handler) {
    struct Hack : Hyprutils::Signal::CSignalBase {
        using CSignalBase::registerListenerInternal;
    };
    return reinterpret_cast<Hack&>(signal).registerListenerInternal([handler](void* args) {
        using Tuple = std::tuple<HyprSignalRefArg<EventType>, HyprSignalRefArg<Event::SCallbackInfo&>>;
        auto* tup = static_cast<Tuple*>(args);
        handler(std::get<0>(*tup), std::get<1>(*tup));
    });
}

inline HANDLE pHandle = NULL;

inline bool compositorUnsafe() {
    return !g_pCompositor || g_pCompositor->m_isShuttingDown;
}

inline PHLMONITOR monitorFromID(MONITORID id) {
    return State::CMonitorQuery(*State::monitorState()).id(id).run();
}

inline PHLMONITOR monitorFromCursor() {
    return State::CMonitorQuery(*State::monitorState()).vec(g_pInputManager->getMouseCoordsInternal()).run();
}

inline PHLMONITOR monitorFromName(const std::string& name) {
    if (name.empty())
        return nullptr;

    return State::CMonitorQuery(*State::monitorState()).name(name).run();
}

inline PHLWORKSPACE workspaceByID(WORKSPACEID id) {
    return State::CWorkspaceQuery(*State::workspaceState()).id(id).run();
}

inline PHLWORKSPACE createWorkspace(WORKSPACEID id, MONITORID monitorID) {
    return State::workspaceState()->create(id, monitorID);
}

inline const std::vector<PHLWINDOW>& windows() {
    return Desktop::windowState()->windows();
}

inline std::vector<PHLWORKSPACE> workspaceList() {
    return State::workspaceState()->workspacesCopy();
}

inline const std::vector<PHLMONITOR>& monitors() {
    return State::monitorState()->monitors();
}

inline void scheduleFrameForMonitor(PHLMONITOR monitor) {
    if (monitor)
        monitor->scheduleFrame();
}

inline void warpCursorTo(const Vector2D& pos) {
    Pointer::pointerController()->warpTo(pos, true);
}

inline PHLWINDOW windowAt(Vector2D coords) {
    const auto& allWindows = windows();
    for (auto it = allWindows.rbegin(); it != allWindows.rend(); ++it) {
        const auto& window = *it;
        if (!window || !window->m_isMapped)
            continue;

        if (window->getWindowBoxUnified(Desktop::View::WINDOW_ONLY).containsPoint(coords))
            return window;
    }

    return nullptr;
}

inline PHLANIMVAR<float>& layerAlpha(PHLLS layer) {
    return layer->alpha()[Desktop::View::LS_ALPHA_FADE];
}

typedef void (*tRenderWindow)(void*, PHLWINDOW, PHLMONITOR, const Time::steady_tp&, bool, Render::eRenderPassMode, bool, bool);
typedef void (*tRenderLayer)(void*, PHLLS, PHLMONITOR, const Time::steady_tp&, bool, bool);
extern void* pRenderWindow;
extern void* pRenderLayer;
namespace Config {
    extern CHyprColor panelBaseColor;
    extern CHyprColor panelBorderColor;
    extern CHyprColor workspaceActiveBackground;
    extern CHyprColor workspaceInactiveBackground;
    extern CHyprColor workspaceActiveBorder;
    extern CHyprColor workspaceInactiveBorder;

    extern int panelHeight;
    extern int panelBorderWidth;
    extern int workspaceMargin;
    extern int reservedArea;
    extern int workspaceBorderSize;
    extern int workspacePreviewCropTop;
    extern bool adaptiveHeight;
    extern bool centerAligned;
    extern bool onBottom;
    extern bool hideBackgroundLayers;
    extern bool hideTopLayers;
    extern bool hideOverlayLayers;
    extern bool drawActiveWorkspace;
    extern bool hideRealLayers;
    extern bool affectStrut;
    extern bool overrideGaps;
    extern int gapsIn;
    extern int gapsOut;

    extern bool autoDrag;
    extern bool autoScroll;
    extern bool exitOnClick;
    extern bool switchOnDrop;
    extern bool exitOnSwitch;
    extern bool showNewWorkspace;
    extern bool showEmptyWorkspace;
    extern bool showSpecialWorkspace;

    extern bool disableGestures;
    extern bool reverseSwipe;

    extern bool disableBlur;
    extern float overrideAnimSpeed;
    extern float dragAlpha;
    extern std::string exitKey;
    extern std::string keepRealLayerNamespaces;

    extern int clickReleaseThresholdMs;
    extern int swipeFingers;
    extern int swipeDistance;
    extern int swipeForceSpeed;
    extern float swipeCancelRatio;
    extern float swipeThreshold;
    extern float swipeClosedPadding;
    extern float workspaceScrollSpeed;
}

extern int numWorkspaces;
