/*
 * SPDX-License-Identifier: AGPL-3.0-only
 */

#pragma once

#include "base_screen.h"

namespace screen {

class WifiScanScreen : public BaseScreen {
public:
    WifiScanScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

protected:
    void build_content(lv_obj_t* content) override;
};

} // namespace screen
