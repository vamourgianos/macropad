/**
 * keymap.c for DWIN macropad
 * ==========================================================
 * Required in rules.mk:
 *   VIA_ENABLE     = yes
 *   ENCODER_ENABLE = yes
 *   RAW_ENABLE     = yes
 */

#include QMK_KEYBOARD_H
// #include "via.h"
// #include "config.h"
#include "macropad_config.h"
#include "dwin_render.h"
#include "dwin_t5uic1.h"
// #include "debug.h"
// #include "hal.h"
// #include "quantum.h"

// ---------------------------------------------------------------------------
// Encoder mode state
// ---------------------------------------------------------------------------

uint8_t          _enc_mode       = ENC_MODE_LAYER;
uint8_t          _cur_layer      = 0;
uint8_t          _color_profile  = 0;
uint8_t          _brightness     = 0xff;
uint8_t          _timeout        = 10;
uint8_t          _display_state  = DISPLAY_ON;
bool             _idle_state     = false;
bool             _display_ok     = false;
static uint32_t  _last_activity  = 0;

settings_state_t _settings  = { .focus = SETTINGS_BRIGHTNESS,
                                .editing = false };

#define MACROPAD_CHANNEL  0x42

// ---------------------------------------------------------------------------
// Keymaps — all layers identical, macros come from EEPROM
// ---------------------------------------------------------------------------
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
    [0] = LAYOUT(MACRO00, MACRO01, MACRO02, MACRO03, MACRO04,
                 MACRO05, MACRO06, MACRO07, MACRO08, MACRO09,
                 MACRO10, MACRO11, MACRO12, MACRO13, MACRO14,
                 MACRO15, MACRO16, MACRO17, MACRO18, MACRO19, ENCMOD
    ),
};

// ---------------------------------------------------------------------------
// Idle timer helpers
// ---------------------------------------------------------------------------

static void _reset_idle_timer(void) {
    _idle_state = false;
    _last_activity = timer_read32();
}

static void _go_idle(void) {
    if (_idle_state) return;
    dprintf("Going idle\n");
    _idle_state = true;
    if (_display_ok) dwin_set_backlight(0);
    backlight_disable();
}

static void _wake_from_idle(void) {
    dprintf("Waking from idle\n");
    _reset_idle_timer();
    if (_display_state == DISPLAY_ON) {
        if (_display_ok) dwin_set_backlight(_brightness);
    }
    backlight_enable();
    // Restore whatever screen was active before idle
    if (_enc_mode == ENC_MODE_SETTINGS) {
        if (_display_ok) dwin_render_settings(&_settings, _brightness, _color_profile, _timeout);
    } else {
        if (_display_ok) dwin_render_layer(_cur_layer);
    }
}

// ---------------------------------------------------------------------------
// Initialisation
// ---------------------------------------------------------------------------
void keyboard_pre_init_kb(void) {
	gpio_set_pin_output(STATUS_LED);
	gpio_write_pin(STATUS_LED, 1); //off

    // How to Disable ST Link and reclaim SWDIO and SWCLK for GPIO
    // Default for F103 is AFIO_MAPR_SWJ_CFG_JTAGDISABLE
    MODIFY_REG(AFIO->MAPR, AFIO_MAPR_SWJ_CFG, AFIO_MAPR_SWJ_CFG_DISABLE);

	keyboard_pre_init_user();
}

void active_init(void) {
    _enc_mode = macropad_config_get_encoder_mode();
    if (_enc_mode == ENC_MODE_SETTINGS) {
        _enc_mode = ENC_MODE_LAYER;
        macropad_config_set_encoder_mode(_enc_mode);
    }
    _cur_layer = macropad_config_get_cur_layer();
    _color_profile = macropad_config_get_color_profile();
    _brightness = macropad_config_get_brightness();
    _timeout = macropad_config_get_timeout();
    _display_state = macropad_config_get_display_state();
    _reset_idle_timer();

    if(_display_state == DISPLAY_ON && _brightness == 0) {
        _brightness = 20;
        macropad_config_set_brightness(_brightness);
    }
}

void keyboard_post_init_user(void) {
    debug_enable=true;
    debug_matrix=true;
    debug_keyboard=true;
    debug_mouse=true;
    // eeconfig_init();
    palSetPadMode(PAL_PORT(BACKLIGHT_PIN), PAL_PAD(BACKLIGHT_PIN), PAL_MODE_STM32_ALTERNATE_OPENDRAIN);
    macropad_config_init();
    active_init();
    _display_ok = dwin_init(DWIN_DIR_LANDSCAPE_0, DWIN_BLACK);
    if (_display_ok) {
	    gpio_write_pin(STATUS_LED, 0); //on
        if (_display_state == DISPLAY_ON) {
            dwin_set_backlight(_brightness);
        } else {
            dwin_set_backlight(0);
        }
        dwin_render_init();
        // we never start on settings
        dwin_render_layer(_cur_layer);
    }
}

// ---------------------------------------------------------------------------
// Housekeeping — runs every QMK scan cycle, checks idle timeout
// ---------------------------------------------------------------------------
void housekeeping_task_user(void) {
    if (_idle_state)                    return; // already idle
    if (_display_state == DISPLAY_OFF)  return; // display manually off, nothing to do
    if (_timeout == 0)                  return; // timmeout disabled

    uint32_t timeout_ms = (uint32_t)_timeout * 60UL * 1000UL;
    if (timer_elapsed32(_last_activity) >= timeout_ms) {
        _go_idle();
    }
}

// ---------------------------------------------------------------------------
// Key handling
// ---------------------------------------------------------------------------
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return true;
    dprintf("Brightness: %d, display state: %d\n", _brightness, _display_state);
    dprintf("Keycode: 0x%04X\n", keycode);
    dprintf("KEYMAP layout: \n");
    dprintf("%04X %04X %04X %04X %04X \n", MACRO00, MACRO01, MACRO02, MACRO03, MACRO04);
    dprintf("%04X %04X %04X %04X %04X \n", MACRO05, MACRO06, MACRO07, MACRO08, MACRO09);
    dprintf("%04X %04X %04X %04X %04X \n", MACRO10, MACRO11, MACRO12, MACRO13, MACRO14);
    dprintf("%04X %04X %04X %04X %04X \n", MACRO15, MACRO16, MACRO17, MACRO18, MACRO19);
    dprintf("%04X \n", ENCMOD);
    dprintf("VIA EEPROM custom config size: %d bytes\n", VIA_EEPROM_CUSTOM_CONFIG_SIZE);
    dprintf("actual config struct size: %d bytes\n", sizeof(macropad_config_t));

    if (_idle_state) {
        _wake_from_idle();
        return false;
    }
    _reset_idle_timer();

    if (keycode >= MACRO00 && keycode <= MACRO19) {
        uint8_t key = keycode - MACRO00;
        macropad_config_exec_macro(_cur_layer, key);
        return false;
    }

    if (keycode == ENCMOD) {
        dprintf("Brightness: %d, display state: %d\n", _brightness, _display_state);
        if (_brightness == 0 && _display_state == DISPLAY_ON) {
            _brightness = 20;
            macropad_config_set_brightness(_brightness);
            if (_display_ok) dwin_set_backlight(_brightness);
        } else if (_display_state == DISPLAY_OFF) {
            dprintf("Turning Display On\n");
            _display_state = DISPLAY_ON;
            macropad_config_set_display_state(_display_state);
            if (_brightness == 0) {
                _brightness = 20;
                macropad_config_set_brightness(_brightness);
            }
            if (_display_ok) dwin_set_backlight(_brightness);
        } else if (_enc_mode == ENC_MODE_SETTINGS) {
            _settings.editing = !_settings.editing;
            if (_settings.focus == SETTINGS_OFF) {
                _display_state = DISPLAY_OFF;
                macropad_config_set_display_state(_display_state);
                if (_display_ok) dwin_set_backlight(0);
                // when OFF, return to layer page
                _enc_mode = ENC_MODE_LAYER;
                macropad_config_set_encoder_mode(_enc_mode);
                if (_display_ok) dwin_render_layer(_cur_layer);
            } else if (_settings.focus == SETTINGS_EXIT || _settings.focus == SETTINGS_NONE) {
                _enc_mode = ((_enc_mode + 1) % ENC_MODE_COUNT);
                macropad_config_set_encoder_mode(_enc_mode);
                if (_display_ok) dwin_render_layer(_cur_layer);
            } else {
                if (_display_ok) dwin_render_settings(&_settings, _brightness, _color_profile, _timeout);
            }
        } else {
            _enc_mode = ((_enc_mode + 1) % ENC_MODE_COUNT);
            macropad_config_set_encoder_mode(_enc_mode);
            if (_enc_mode == ENC_MODE_LAYER) {
                if (_display_ok) dwin_render_layer(_cur_layer);
            } else if (_enc_mode == ENC_MODE_SETTINGS) {
                _settings.focus   = SETTINGS_NONE;
                _settings.editing = false;
                if (_display_ok) dwin_render_settings(&_settings, _brightness, _color_profile, _timeout);
            } else {
                if (_display_ok) dwin_render_mode_badge(_enc_mode);
            }
        }
        return false;
    }

    return true;
}

// ---------------------------------------------------------------------------
// Encoder handling
// ---------------------------------------------------------------------------
bool encoder_update_user(uint8_t index, bool clockwise) {
    // Any encoder movement wakes the device; the triggering tick is consumed.
    if (_idle_state) {
        _wake_from_idle();
        return false;
    }
    _reset_idle_timer();

    switch (_enc_mode) {
        case ENC_MODE_LAYER:
            if (clockwise)
                _cur_layer = (_cur_layer + 1) % MACROPAD_NUM_LAYERS;
            else
                _cur_layer = (_cur_layer + MACROPAD_NUM_LAYERS - 1) % MACROPAD_NUM_LAYERS;
            macropad_config_set_cur_layer(_cur_layer);
            if (_display_ok) dwin_change_layer(_cur_layer);
            break;

        case ENC_MODE_VOLUME:
            tap_code(clockwise ? KC_VOLU : KC_VOLD);
            break;

        case ENC_MODE_MACRO:
            macropad_config_exec_enc_macro(clockwise);
            break;

        case ENC_MODE_SETTINGS:
            if (_settings.editing) {
                if (_settings.focus == SETTINGS_BRIGHTNESS) {
                    if (clockwise)
                        _brightness = (_brightness < 235) ? _brightness + 20 : 0xFF;
                    else
                        _brightness = (_brightness > 20)  ? _brightness - 20 : 0x00;
                    macropad_config_set_brightness(_brightness);
                    if (_display_ok) dwin_set_backlight(_brightness);
                    dwin_update_brightness(&_settings, _brightness);
                } else if (_settings.focus == SETTINGS_PROFILE) {
                    if (clockwise)
                        _color_profile = (_color_profile + 1) % COLOR_PROFILE_COUNT;
                    else
                        _color_profile = (_color_profile + COLOR_PROFILE_COUNT - 1) % COLOR_PROFILE_COUNT;
                    macropad_config_set_color_profile(_color_profile);
                    if (_display_ok) dwin_render_settings(&_settings, _brightness, _color_profile, _timeout);
                } else if (_settings.focus == SETTINGS_TIMEOUT) {
                    if (clockwise)
                        _timeout = (_timeout < 60) ? _timeout + 1 : 0;  //rollover
                    else
                        _timeout = (_timeout > 0)  ? _timeout - 1 : 60; //rollover
                    macropad_config_set_timeout(_timeout);
                    dwin_update_timeout(&_settings, _timeout);
                } else if (_settings.focus == SETTINGS_BACKLIGHT) {
                    if (clockwise) backlight_increase();
                    else           backlight_decrease();
                    dwin_update_backlight(&_settings);
                }
            } else {
                if (_settings.focus == SETTINGS_NONE) {
                    _settings.focus = SETTINGS_EXIT;
                }
                if (clockwise)
                    _settings.focus = (settings_item_t)
                                      ((_settings.focus + 1) % SETTINGS_NUM);
                else
                    _settings.focus = (settings_item_t)
                                      ((_settings.focus + SETTINGS_NUM - 1)
                                       % SETTINGS_NUM);
                if (_display_ok) dwin_render_settings(&_settings, _brightness, _color_profile, _timeout);
            }
            break;

        default: break;
    }
    return false;
}

// ---------------------------------------------------------------------------
// VIA custom command handler
// ---------------------------------------------------------------------------
void via_custom_value_command_kb(uint8_t *data, uint8_t length) {
    if (data[1] != 0x00) return; // only one channel (0) supported
    macropad_config_via_command(&data[0], length);
    // sync with config
    _enc_mode       = macropad_config_get_encoder_mode();
    _cur_layer      = macropad_config_get_cur_layer();
    _color_profile  = macropad_config_get_color_profile();
    _brightness     = macropad_config_get_brightness();
    _timeout        = macropad_config_get_timeout();
    _display_state  = macropad_config_get_display_state();
    return;
}
