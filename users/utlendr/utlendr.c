/* Copyright 2019 Reed Bell
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "utlendr.h"
#include "version.h"


#if defined(RGBLIGHT_ENABLE)
extern rgblight_config_t rgblight_config;
#elif defined(RGB_MATRIX_ENABLE)
extern rgb_config_t rgb_matrix_config;
#endif

#if defined(RGB_IDLE)
//#define IDLE_TIMEOUT 30000   // Blank the rgb lighting after 30 secs of inactivity
#define IDLE_TIMEOUT 600000   // Blank the rgb lighting after 600 secs of inactivity
static uint32_t rgb_idle_timer;
static bool rgb_idle_timeout = false;
#if (defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE))
uint8_t rgb_idle_mode = 0; // 0 means idle will disable lighting, otherwise specify the mode to use
static uint8_t rgb_prev_mode = 0; // Used to restore previous lighting mode when idle deactivates.
#endif
#endif


__attribute__((weak))
void keyboard_post_init_keymap(void){ }

void keyboard_post_init_user(void) {
#if defined(RGB_IDLE)
    rgb_idle_timer = timer_read32();
    // Set default idle mode if desired
/* #if defined(RGB_MATRIX_ENABLE) */
/*     rgb_idle_mode = RGB_MATRIX_DIGITAL_RAIN; */
/* #endif */
#endif
    keyboard_post_init_keymap();
}

#if defined(RGB_IDLE)
void rgb_idle_set_mode(void) {
#if defined(RGBLIGHT_ENABLE)
    rgb_idle_mode = rgblight_config.enable ? rgblight_config.mode : 0;
#elif defined(RGB_MATRIX_ENABLE)
    rgb_idle_mode = rgb_matrix_config.enable ? rgb_matrix_get_mode() : 0;
#endif
}
bool rgb_idle_can_activate(void) {
#if defined(RGBLIGHT_ENABLE)
    return rgblight_config.enable
        && (rgb_idle_mode == 0 || rgblight_config.mode != rgb_idle_mode);
#elif defined(RGB_MATRIX_ENABLE)
    return rgb_matrix_config.enable
        && (rgb_idle_mode == 0 || rgb_matrix_get_mode() != rgb_idle_mode);
#else
    return true;
#endif
}
void rgb_idle_activate(void) {
#if defined(RGBLIGHT_ENABLE)
    if (rgb_idle_mode) {
        rgb_prev_mode = rgblight_config.mode;
        rgblight_mode_noeeprom(rgb_idle_mode);
    } else {
        rgblight_disable_noeeprom();
    }
#elif defined(RGB_MATRIX_ENABLE)
    if (rgb_idle_mode) {
        rgb_prev_mode = rgb_matrix_get_mode();
        rgb_matrix_mode(rgb_idle_mode);
    } else {
        rgb_matrix_disable_noeeprom();
    }
#endif
}
void rgb_idle_restore(void) {
#if defined(RGBLIGHT_ENABLE)
    if (rgb_idle_mode) {
        rgblight_mode_noeeprom(rgb_prev_mode);
    } else {
        rgblight_enable_noeeprom();
    }
#elif defined(RGB_MATRIX_ENABLE)
    if (rgb_idle_mode) {
        rgb_matrix_mode(rgb_prev_mode);
    } else {
        rgb_matrix_enable_noeeprom();
    }
#endif
}
#endif

__attribute__ ((weak))
void matrix_scan_keymap(void) {}

void matrix_scan_user(void) {
#if defined(RGB_IDLE)
    if (!rgb_idle_timeout && rgb_idle_can_activate()) {
        bool shouldblank = timer_elapsed32(rgb_idle_timer) < IDLE_TIMEOUT ? false : true;
        if (shouldblank) {
            //xprintf("rgb_idle_timeout: activating\n");
            rgb_idle_activate();
            rgb_idle_timeout = true;
        }
    }
#endif
    matrix_scan_keymap();
}


__attribute__ ((weak))
layer_state_t layer_state_set_keymap(layer_state_t state) {
    return state;
}

layer_state_t layer_state_set_user(layer_state_t state) {
    state = update_tri_layer_state(state, _LOWER, _RAISE, _ADJUST);
    return layer_state_set_keymap(state);
}


__attribute__ ((weak))
bool process_record_keymap(uint16_t keycode, keyrecord_t *record) {
    return true;
}

bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#if defined(RGB_IDLE)
    if (record->event.pressed) {
        if (rgb_idle_timeout) {
            //xprintf("rgb_idle_timeout: restoring\n");
            rgb_idle_restore();
            rgb_idle_timeout = false;
        }
        rgb_idle_timer = timer_read32();
    }
#endif
    switch (keycode) {
    case MAKE:
        if (!record->event.pressed) {
            uint8_t mods = get_mods();
            clear_mods();
            SEND_STRING("make " QMK_KEYBOARD ":" QMK_KEYMAP);
            if (mods & MOD_MASK_SHIFT) {
#if defined(__arm__)
                SEND_STRING(":dfu-util");
#elif  (defined(BOOTLOADER_DFU) || defined(BOOTLOADER_LUFA_DFU) || defined(BOOTLOADER_QMK_DFU))
                SEND_STRING(":dfu");
#elif defined(BOOTLOADER_HALFKAY)
                SEND_STRING(":teensy");
#elif defined(BOOTLOADER_CATERINA)
                SEND_STRING(":avrdude");
#endif
            }
#ifdef RGB_MATRIX_ENABLE
#ifdef RGB_MATRIX_SPLIT_RIGHT
                SEND_STRING(" RGB_MATRIX_SPLIT_RIGHT=yes");
#endif
#endif
            set_mods(mods);
        }
        return false;
        break;
    case VRSN: // Prints firmware version
        if (record->event.pressed) {
            //send_string_with_delay_P(PSTR(QMK_KEYBOARD "/" QMK_KEYMAP " @ " QMK_VERSION ", Built on: " QMK_BUILDDATE), MACRO_TIMER);
            SEND_STRING(QMK_KEYBOARD "/" QMK_KEYMAP " version " QMK_VERSION " built on " QMK_BUILDDATE);
        }
        return false;
        break;
    case RGBRST:
#ifdef RGBLIGHT_ENABLE
        if (record->event.pressed) {
            eeconfig_update_rgblight_default();
            rgblight_enable();
        }
#endif
        return false;
        break;
    case RGBISET: // Set the mode used by my rgb_idle timeout handler
#ifdef RGB_IDLE
        if (record->event.pressed) {
            rgb_idle_set_mode();
        }
#endif
        return false;
        break;
    }
    return process_record_keymap(keycode, record);
}
