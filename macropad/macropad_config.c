/**
 * macropad_config.c — Display Layout Config for DWIN T5UIC1 + VIA
 * ===============================================================
 * See macropad_config.h for full documentation.
 */

#include "macropad_config.h"
#include "dwin_render.h"
#include "via.h"
#include "wait.h"
#include "debug.h"

// ---------------------------------------------------------------------------
// RAM cache
// ---------------------------------------------------------------------------

static macropad_config_t _config;
extern uint8_t           _enc_mode;
extern uint8_t           _cur_layer;
extern uint8_t           _color_profile;
extern uint8_t           _brightness;
extern uint8_t           _timeout;
extern bool              _display_ok;

extern settings_state_t  _settings;
// ---------------------------------------------------------------------------
// EEPROM helpers
// ---------------------------------------------------------------------------

static void _load_from_eeprom(void) {
    via_read_custom_config(&_config, 0, sizeof(macropad_config_t));
}

static void _save_to_eeprom(void) {
    via_update_custom_config(&_config, 0, sizeof(macropad_config_t));
}

static void _save_layer_name_to_eeprom(uint8_t layer) {
    uintptr_t offset = sizeof(active_config_t)
                     + layer * sizeof(layer_config_t);
    via_update_custom_config(&_config.layers[layer].layer_name, offset, MACROPAD_NAME_LEN);
}

static void _save_key_label_to_eeprom(uint8_t layer, uint8_t key) {
    uintptr_t offset = sizeof(active_config_t)
                     + layer * sizeof(layer_config_t)
                     + MACROPAD_NAME_LEN
                     + key * sizeof(key_config_t);
    via_update_custom_config(&_config.layers[layer].keys[key].label, offset, MACROPAD_LABEL_LEN);
}

static void _save_key_macro_to_eeprom(uint8_t layer, uint8_t key) {
    uintptr_t offset = sizeof(active_config_t)
                     + layer * sizeof(layer_config_t)
                     + MACROPAD_NAME_LEN
                     + key * sizeof(key_config_t)
                     + MACROPAD_LABEL_LEN;
    via_update_custom_config(&_config.layers[layer].keys[key].macro, offset, sizeof(macro_t));
}

static void _save_encoder_key_label_to_eeprom(uint8_t key) {
    uintptr_t offset = sizeof(active_config_t)
                     + MACROPAD_NUM_LAYERS * sizeof(layer_config_t)
                     + key*sizeof(key_config_t);
    via_update_custom_config(&_config.enc_macro[key].label, offset, MACROPAD_LABEL_LEN);
}

static void _save_encoder_key_macro_to_eeprom(uint8_t key) {
    uintptr_t offset = sizeof(active_config_t)
                     + MACROPAD_NUM_LAYERS * sizeof(layer_config_t)
                     + key*sizeof(key_config_t)
                     + MACROPAD_LABEL_LEN;
    via_update_custom_config(&_config.enc_macro[key].macro, offset, sizeof(macro_t));
}

static void _save_active_to_eeprom(void) {
    via_update_custom_config(&_config.active, 0, sizeof(active_config_t));
}

static bool _config_is_blank(void) {
    for (uint8_t i = 0; i < MACROPAD_NAME_LEN; i++) {
        if ((uint8_t)_config.layers[0].layer_name[i] != 0xFF) return false;
    }
    return true;
}

static void _write_defaults(void) {
    _config.active.encoder_mode  = ENC_MODE_LAYER;
    _config.active.cur_layer     = 0;
    _config.active.color_profile = PROFILE_OCEAN;
    _config.active.brightness    = 0xFF;
    _config.active.display_state = DISPLAY_ON;
    const char *default_names[MACROPAD_NUM_LAYERS] = {
        "LAYER 0", "LAYER 1", "LAYER 2", "LAYER 3"
    };
    memset(&_config, 0, sizeof(_config));
    for (uint8_t l = 0; l < MACROPAD_NUM_LAYERS; l++) {
        strncpy(_config.layers[l].layer_name, default_names[l],MACROPAD_NAME_LEN);
        // Keys default to empty label, no macro
        for (uint8_t k = 0; k < MACROPAD_NUM_KEYS; k++) {
            _config.layers[l].keys[k].label[0] = '\0';
            _config.layers[l].keys[k].macro.count = 0;
        }
    }
    for (uint8_t k = 0; k < 2; k++) {
        _config.enc_macro[k].label[0] = '\0';
        _config.enc_macro[k].macro.count = 0;
    }
    _save_to_eeprom();
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void macropad_config_init(void) {
    _load_from_eeprom();
    if (_config_is_blank()) _write_defaults();
    dprintf("_enc_mode: %d\n", _config.active.encoder_mode);
    dprintf("_cur_layer: %d, _color_profile: %d\n", _config.active.cur_layer, _config.active.color_profile);
    dprintf("_brightness: %d, _display_state: %d\n", _config.active.brightness, _config.active.display_state);
}

const layer_config_t *macropad_config_get_layer(uint8_t layer) {
    if (layer >= MACROPAD_NUM_LAYERS) return NULL;
    return &_config.layers[layer];
}

const key_config_t *macropad_config_get_key(uint8_t layer, uint8_t key) {
    if (layer >= MACROPAD_NUM_LAYERS) return NULL;
    if (key   >= MACROPAD_NUM_KEYS)   return NULL;
    return &_config.layers[layer].keys[key];
}

uint8_t macropad_config_get_encoder_mode(void) {
    return _config.active.encoder_mode;
}

const key_config_t* macropad_config_get_encoder_macros(void) {
    return _config.enc_macro;
}

uint8_t macropad_config_get_cur_layer(void) {
    return _config.active.cur_layer;
}

uint8_t macropad_config_get_color_profile(void) {
    return _config.active.color_profile;
}

const color_profile_t *macropad_config_get_profile(void) {
    uint8_t id = _config.active.color_profile;
    if (id >= COLOR_PROFILE_COUNT) id = 0;
    return &COLOR_PROFILES[id];
}

uint8_t macropad_config_get_brightness(void) {
    return _config.active.brightness;
}

uint8_t macropad_config_get_timeout(void) {
    return _config.active.timeout;
}

uint8_t macropad_config_get_display_state(void) {
    return _config.active.display_state;
}

void macropad_config_set_encoder_mode(uint8_t encoder_mode) {
    _config.active.encoder_mode = encoder_mode;
    _save_active_to_eeprom();
}

void macropad_config_set_cur_layer(uint8_t cur_layer) {
    _config.active.cur_layer = cur_layer;
    _save_active_to_eeprom();
}

void macropad_config_set_color_profile(uint8_t profile_id) {
    _config.active.color_profile = profile_id;
    _save_active_to_eeprom();
}
    
void macropad_config_set_brightness(uint8_t brightness) {
    _config.active.brightness = brightness;
    _save_active_to_eeprom();
}
    
void macropad_config_set_timeout(uint8_t timeout) {
    if (timeout > 60) _config.active.timeout = 60;
    else              _config.active.timeout = timeout;
    _save_active_to_eeprom();
}
    
void macropad_config_set_display_state(uint8_t state) {
    _config.active.display_state = state;
    _save_active_to_eeprom();
}

void macropad_config_reset(void) {
    // eeconfig_init(); // Clear EEPROM
    via_eeprom_set_valid(false);
    eeconfig_init_via();
    _write_defaults();
}

// ---------------------------------------------------------------------------
// Macro execution
// ---------------------------------------------------------------------------
void macropad_config_exec_macro(uint8_t layer, uint8_t key) {
    dprintf("Executing macro for layer %d key %d\n", layer, key);
    if (layer >= MACROPAD_NUM_LAYERS) return;
    if (key   >= MACROPAD_NUM_KEYS)   return;

    const macro_t *m = &_config.layers[layer].keys[key].macro;
    dprintf("Macro has %d steps\n", m->count);
    if (m->count == 0) return;
    for (uint8_t i = 0; i < m->count && i < MACRO_STEPS; i++) {
        uint8_t mods    = m->steps[i].mods;
        uint8_t keycode = m->steps[i].keycode;
        dprintf("Executing macro step %d: mods=0x%02X keycode=0x%02X\n", i, mods, keycode);
        if (mods)    register_mods(mods);
        if (keycode) register_code(keycode);
        wait_ms(10);
        if (keycode) unregister_code(keycode);
        if (mods)    unregister_mods(mods);
        wait_ms(10);
    }
}

void macropad_config_exec_enc_macro(bool clockwise) {
    const macro_t *m = &_config.enc_macro[clockwise].macro;
    dprintf("Macro has %d steps\n", m->count);
    if (m->count == 0) return;
    for (uint8_t i = 0; i < m->count && i < MACRO_STEPS; i++) {
        uint8_t mods    = m->steps[i].mods;
        uint8_t keycode = m->steps[i].keycode;
        dprintf("Executing macro step %d: mods=0x%02X keycode=0x%02X\n", i, mods, keycode);
        if (mods)    register_mods(mods);
        if (keycode) register_code(keycode);
        wait_ms(10);
        if (keycode) unregister_code(keycode);
        if (mods)    unregister_mods(mods);
        wait_ms(10);
    }
}

// ---------------------------------------------------------------------------
// VIA custom command handler
// ---------------------------------------------------------------------------

void macropad_config_via_command(uint8_t *data, uint8_t length) {
    uint8_t sub_cmd = data[2];
    uint8_t layer   = data[3];
    uint8_t key     = data[4];

    dprintf("VIA command: %x (%d), layer=%d, key=%d (length %d)\n", sub_cmd, sub_cmd, layer, key, length);
    switch (sub_cmd) {
        case VIA_GET_SIZES: {
            data[3] = MACROPAD_NUM_LAYERS;
            data[4] = MACROPAD_NAME_LEN;
            data[5] = MACROPAD_NUM_ROWS;
            data[6] = MACROPAD_NUM_COLUMNS;
            data[7] = MACROPAD_LABEL_LEN;
            data[8] = MACRO_STEPS;
            break;
        }
        case VIA_GET_LAYER_NAME: {
            if (layer >= MACROPAD_NUM_LAYERS) break;
            memcpy(&data[4], _config.layers[layer].layer_name, MACROPAD_NAME_LEN);
            break;
        }
        case VIA_SET_LAYER_NAME: {
            if (layer >= MACROPAD_NUM_LAYERS) break;
            memcpy(_config.layers[layer].layer_name, &data[4], MACROPAD_NAME_LEN);
            _save_layer_name_to_eeprom(layer);
            break;
        }
        case VIA_GET_KEY_LABEL: {
            if (layer >= MACROPAD_NUM_LAYERS) break;
            if (key   >= MACROPAD_NUM_KEYS)   break;
            memcpy(&data[5], &_config.layers[layer].keys[key].label, MACROPAD_LABEL_LEN);
            break;
        }
        case VIA_SET_KEY_LABEL: {
            if (layer >= MACROPAD_NUM_LAYERS) break;
            if (key   >= MACROPAD_NUM_KEYS)   break;
            memcpy(&_config.layers[layer].keys[key].label, &data[5], MACROPAD_LABEL_LEN);
            _save_key_label_to_eeprom(layer, key);
            break;
        }
        case VIA_GET_KEY_MACRO: {
            if (layer >= MACROPAD_NUM_LAYERS) break;
            if (key   >= MACROPAD_NUM_KEYS)   break;
            memcpy(&data[5], &_config.layers[layer].keys[key].macro, sizeof(macro_t));
            break;
        }
        case VIA_SET_KEY_MACRO: {
            if (layer >= MACROPAD_NUM_LAYERS) break;
            if (key   >= MACROPAD_NUM_KEYS)   break;
            memcpy(&_config.layers[layer].keys[key].macro, &data[5], sizeof(macro_t));
            _save_key_macro_to_eeprom(layer, key);
            break;
        }
        case VIA_GET_ENC_KEY_LABEL: {
            if (layer >= 2) break;
            memcpy(&data[4], &_config.enc_macro[layer].label, MACROPAD_LABEL_LEN);
            break;
        }
        case VIA_SET_ENC_KEY_LABEL: {
            if (layer >= 2) break;
            memcpy(&_config.enc_macro[layer].label, &data[4], MACROPAD_LABEL_LEN);
            _save_encoder_key_label_to_eeprom(layer);
            break;
        }
        case VIA_GET_ENC_KEY_MACRO: {
            if (layer >= 2) break;
            memcpy(&data[4], &_config.enc_macro[layer].macro, sizeof(macro_t));
            break;
        }
        case VIA_SET_ENC_KEY_MACRO: {
            if (layer >= 2) break;
            memcpy(&_config.enc_macro[layer].macro, &data[4], sizeof(macro_t));
            _save_encoder_key_macro_to_eeprom(layer);
            break;
        }
        case VIA_RESET_CONFIG: {
            macropad_config_reset();
            break;
        }
        case VIA_GET_ACTIVE: {
            data[3] = _config.active.encoder_mode;
            data[4] = _config.active.cur_layer;
            data[5] = _config.active.color_profile;
            data[6] = _config.active.brightness;
            data[7] = _config.active.timeout;
            data[8] = _config.active.display_state;
            break;
        }
        case VIA_SET_ACTIVE: {
            _config.active.encoder_mode  = data[3];
            _config.active.cur_layer     = data[4];
            _config.active.color_profile = data[5];
            _config.active.brightness    = data[6];
            _config.active.timeout       = data[7];
            _config.active.display_state = data[8];
            _save_active_to_eeprom();
            break;
        }
        case VIA_REDRAW_DISPLAY: {
            if (_enc_mode == ENC_MODE_SETTINGS) {
                if (_display_ok) dwin_render_settings(&_settings, _brightness, _color_profile, _timeout);
            } else {
                if (_display_ok) dwin_render_layer(_cur_layer);
            }
            break;
        }
        default: break;
    }
}
