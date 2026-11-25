// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"
#include <array>

namespace screen {
class RadioSetupScreen : public BaseScreen {
public:
    RadioSetupScreen(viewmodel::BaseViewModel&, app::AssetManager&);
private:
    struct Row { lv_obj_t *box, *icon, *name, *state; };
    std::array<Row, 4> rows_{};
    lv_obj_t *heading_{}, *count_{}, *type_{}, *product_{}, *location_{}, *driver_{}, *detail_{};
    void build_content(lv_obj_t*) override;
    void render();
    static void changed(lv_observer_t*, lv_subject_t*);
};
}
