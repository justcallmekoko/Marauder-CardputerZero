#include "shortcut_policy.h"
#include "layout_metrics.h"

#include <cassert>

int main() {
    using app::EscapeAction;
    using app::NavigationAction;
    using app::escape_action;
    using app::navigation_action;
    using model::AppPage;

    assert(escape_action(AppPage::Dashboard, true) == EscapeAction::ExitApplication);
    assert(escape_action(AppPage::Dashboard, false) == EscapeAction::BackMenu);
    assert(escape_action(AppPage::Adapters, false) == EscapeAction::ShowDashboard);
    assert(escape_action(AppPage::WifiScan, false) == EscapeAction::ShowDashboard);
    assert(escape_action(AppPage::Diagnostics, false) == EscapeAction::ShowDashboard);

    assert(navigation_action('f') == NavigationAction::Previous);
    assert(navigation_action('x') == NavigationAction::Next);
    assert(navigation_action('c') == NavigationAction::Primary);
    assert(navigation_action('z') == NavigationAction::Back);
    assert(navigation_action('C') == NavigationAction::Primary);
    assert(navigation_action('Z') == NavigationAction::Back);
    assert(navigation_action('d') == NavigationAction::Details);
    for (uint32_t key = '4'; key <= '8'; ++key) {
        assert(navigation_action(key) == NavigationAction::None);
    }

    static_assert(view::kContentHeight == 124, "reserve room for status and shortcut bars");
    return 0;
}
