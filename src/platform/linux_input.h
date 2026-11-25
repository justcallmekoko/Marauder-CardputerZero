#pragma once

#include "lvgl.h"

#include <cstdint>

namespace platform {

constexpr uint32_t kSpecialKeyBase = 0x200000U;
constexpr uint32_t kKeyPrintScreen = kSpecialKeyBase | 18U;
constexpr uint32_t kKeyHelp = kSpecialKeyBase | 19U;

using GlobalKeyListener = bool (*)(uint32_t key,
                                   const char* key_name,
                                   bool long_pressed,
                                   void* user_data);

void init_key_input(lv_display_t* display);
void reset_key_input_state();
void attach_key_router(lv_indev_t* indev);
void set_global_key_listener(GlobalKeyListener listener, void* user_data);
void clear_global_key_listener(GlobalKeyListener listener, void* user_data);
const char* describe_key(uint32_t key);

} // namespace platform
