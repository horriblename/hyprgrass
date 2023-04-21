#include "gestures.hpp"
#include "src/Compositor.hpp"
#include "src/managers/input/InputManager.hpp"

// returns whether or not to inhibit further actions
void CGestures::onTouchDown(wlr_touch_down_event* e) {
    if (e->touch_id == 0) {
        return;
    }
    auto PMONITOR = g_pCompositor->getMonitorFromName(e->touch->output_name ? e->touch->output_name : "");

    const auto PDEVIT = std::find_if(g_pInputManager->m_lTouchDevices.begin(), g_pInputManager->m_lTouchDevices.end(), [&](const STouchDevice& other) { return other.pWlrDevice == &e->touch->base; });

    if (PDEVIT != g_pInputManager->m_lTouchDevices.end() && !PDEVIT->boundOutput.empty())
        PMONITOR = g_pCompositor->getMonitorFromName(PDEVIT->boundOutput);

    PMONITOR = PMONITOR ? PMONITOR : g_pCompositor->m_pLastMonitor;

    wlr_cursor_warp(g_pCompositor->m_sWLRCursor, nullptr, PMONITOR->vecPosition.x + e->x * PMONITOR->vecSize.x, PMONITOR->vecPosition.y + e->y * PMONITOR->vecSize.y);

    g_pInputManager->refocus();

    g_pInputManager->m_bLastInputTouch = true;

    // g_pInputManager->m_sTouchData.touchFocusWindow = g_pInputManager->m_pFoundWindowToFocus;
    // g_pInputManager->m_sTouchData.touchFocusSurface = g_pInputManager->m_pFoundSurfaceToFocus;
    // g_pInputManager->m_sTouchData.touchFocusLS = g_pInputManager->m_pFoundLSToFocus;
    //
    // Vector2D local;
    //
    // if (g_pInputManager->m_sTouchData.touchFocusWindow) {
    //     if (g_pInputManager->m_sTouchData.touchFocusWindow->m_bIsX11) {
    //         local = g_pInputManager->getMouseCoordsInternal() - g_pInputManager->m_sTouchData.touchFocusWindow->m_vRealPosition.goalv();
    //     } else {
    //         g_pCompositor->vectorWindowToSurface(g_pInputManager->getMouseCoordsInternal(), g_pInputManager->m_sTouchData.touchFocusWindow, local);
    //     }
    //
    //     g_pInputManager->m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;
    // } else if (g_pInputManager->m_sTouchData.touchFocusLS) {
    //     local = g_pInputManager->getMouseCoordsInternal() - Vector2D(g_pInputManager->m_sTouchData.touchFocusLS->geometry.x, g_pInputManager->m_sTouchData.touchFocusLS->geometry.y) -
    //             g_pCompositor->m_pLastMonitor->vecPosition;
    //
    //     g_pInputManager->m_sTouchData.touchSurfaceOrigin = g_pInputManager->getMouseCoordsInternal() - local;
    // } else {
    //     return; // oops, nothing found.
    // }
    //
    // wlr_seat_touch_notify_down(g_pCompositor->m_sSeat.seat, g_pInputManager->m_sTouchData.touchFocusSurface, e->time_msec, e->touch_id, local.x, local.y);
    //
    // wlr_idle_notify_activity(g_pCompositor->m_sWLRIdle, g_pCompositor->m_sSeat.seat);
}
