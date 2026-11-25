/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "base_screen.h"

namespace screen {

class DiagnosticsScreen : public BaseScreen {
public:
    DiagnosticsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~DiagnosticsScreen() override;

private:
    void build_content(lv_obj_t* content) override;
    static void diagnostics_changed_cb(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* scroll_{nullptr};
    lv_observer_t* diagnostics_observer_{nullptr};
};

} // namespace screen
