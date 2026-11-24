/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_model.h"

namespace app {

enum class EscapeAction {
    ExitApplication,
    BackMenu,
    ShowDashboard,
};

constexpr EscapeAction escape_action(model::AppPage page, bool dashboard_at_root) {
    if (page != model::AppPage::Dashboard) {
        return EscapeAction::ShowDashboard;
    }
    return dashboard_at_root ? EscapeAction::ExitApplication : EscapeAction::BackMenu;
}

} // namespace app
