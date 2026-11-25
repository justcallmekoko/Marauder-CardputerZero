/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"

#include <cstdint>

namespace app {

enum class EscapeAction {
    ExitApplication,
    BackMenu,
    ShowDashboard,
};

enum class NavigationAction {
    None,
    Previous,
    Next,
    Primary,
    Back,
    Details,
};

constexpr NavigationAction navigation_action(uint32_t key) {
    switch (key) {
        case 'f':
        case 'F':
            return NavigationAction::Previous;
        case 'x':
        case 'X':
            return NavigationAction::Next;
        case 'c':
        case 'C':
            return NavigationAction::Primary;
        case 'z':
        case 'Z':
            return NavigationAction::Back;
        case 'd':
        case 'D':
            return NavigationAction::Details;
        default:
            return NavigationAction::None;
    }
}

constexpr EscapeAction escape_action(model::AppPage page, bool dashboard_at_root) {
    if (page != model::AppPage::Dashboard) {
        return EscapeAction::ShowDashboard;
    }
    return dashboard_at_root ? EscapeAction::ExitApplication : EscapeAction::BackMenu;
}

} // namespace app
