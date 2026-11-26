// SPDX-License-Identifier: AGPL-3.0-only
#pragma once
#include "base_screen.h"

namespace screen {
class GpsToolScreen : public BaseScreen {
public:
    GpsToolScreen(viewmodel::BaseViewModel& model, app::AssetManager& assets);
private:
    void build_content(lv_obj_t* content) override;
    void render();
    lv_obj_t *heading_{}, *state_{}, *status_{}, *position_{}, *detail_{}, *message_{}, *file_{};
};
}
