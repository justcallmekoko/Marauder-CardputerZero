#include "shortcut_policy.h"

#include <cassert>

int main() {
    using app::EscapeAction;
    using app::escape_action;
    using model::AppPage;

    assert(escape_action(AppPage::Dashboard, true) == EscapeAction::ExitApplication);
    assert(escape_action(AppPage::Dashboard, false) == EscapeAction::BackMenu);
    assert(escape_action(AppPage::Adapters, false) == EscapeAction::ShowDashboard);
    assert(escape_action(AppPage::WifiScan, false) == EscapeAction::ShowDashboard);
    assert(escape_action(AppPage::Diagnostics, false) == EscapeAction::ShowDashboard);
    return 0;
}
