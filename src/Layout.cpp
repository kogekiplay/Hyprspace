#include "Overview.hpp"
#include "Globals.hpp"

#include <algorithm>
#include <optional>
#include <string>
#include <utility>

#include <hyprland/src/config/legacy/ConfigManager.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>

namespace {

std::optional<std::pair<std::string, std::string>> currentGlobalGapRules() {
    static auto gapsInData  = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
    static auto gapsOutData = CConfigValue<Config::IComplexConfigValue>("general:gaps_out");
    if (!gapsInData.good() || !gapsOutData.good())
        return std::nullopt;

    const auto gapsInBase  = gapsInData.ptr();
    const auto gapsOutBase = gapsOutData.ptr();
    if (!gapsInBase || !gapsOutBase || gapsInBase->getDataType() != Config::CVD_TYPE_CSS_VALUE || gapsOutBase->getDataType() != Config::CVD_TYPE_CSS_VALUE)
        return std::nullopt;

    const auto* gapsIn  = static_cast<Config::CCssGapData*>(gapsInBase);
    const auto* gapsOut = static_cast<Config::CCssGapData*>(gapsOutBase);
    return std::make_pair(gapsIn->toString(), gapsOut->toString());
}

bool applyWorkspaceGaps(WORKSPACEID id, const std::string& gapsIn, const std::string& gapsOut) {
    const auto legacy = Config::Legacy::mgr().lock();
    if (!legacy)
        return false;

    const auto rule = std::to_string(id) + ", gapsin:" + gapsIn + ", gapsout:" + gapsOut;
    legacy->handleWorkspaceRules("", rule);
    return true;
}

} // namespace

void CHyprspaceWidget::updateLayout() {
    const auto monitor = getOwner();
    if (!monitor)
        return;

    const auto reservedHeight = std::max(0, Config::panelHeight + Config::reservedArea);
    if (active && Config::affectStrut) {
        monitor->m_reservedArea = Config::onBottom ? Desktop::CReservedArea(0, 0, reservedHeight, 0) : Desktop::CReservedArea(reservedHeight, 0, 0, 0);
    } else {
        monitor->m_reservedArea = Desktop::CReservedArea();
    }

    g_pHyprRenderer->arrangeLayersForMonitor(ownerID);

    if (Config::overrideGaps) {
        const auto globalGaps = currentGlobalGapRules();
        if (globalGaps) {
            if (active) {
                const auto activeWorkspace = monitor->m_activeWorkspace;
                if (activeWorkspace) {
                    for (const auto& workspace : workspaceList()) {
                        if (!workspace || !workspace->m_monitor || workspace->m_monitor->m_id != ownerID || workspace->m_id == activeWorkspace->m_id)
                            continue;

                        monitor->m_activeWorkspace = workspace;
                        applyWorkspaceGaps(workspace->m_id, globalGaps->first, globalGaps->second);
                        g_layoutManager->recalculateMonitor(monitor);
                    }

                    monitor->m_activeWorkspace = activeWorkspace;
                    applyWorkspaceGaps(monitor->activeWorkspaceID(), std::to_string(Config::gapsIn), std::to_string(Config::gapsOut));
                }
            } else {
                for (const auto& workspace : workspaceList()) {
                    if (!workspace || !workspace->m_monitor || workspace->m_monitor->m_id != ownerID)
                        continue;

                    applyWorkspaceGaps(workspace->m_id, globalGaps->first, globalGaps->second);
                }
            }
        }
    }

    g_layoutManager->recalculateMonitor(monitor);
    scheduleFrameForMonitor(monitor);
}
