/**
 * macropad_config.h — Display Layout Config for DWIN T5UIC1 + VIA
 * ===============================================================
 * Stores per-layer display labels and key macros in QMK emulated EEPROM.
 * Editable via VIA custom UI without reflashing.
 *
 * Per-layer config (268 bytes):
 *   layer_name[16]       Layer name e.g. "AUTOCAD"
 *   keys[20]             21 bytes each:
 *     label[8]             Display label e.g. "Line"
 *     macro:
 *       steps[6]           Up to 6 keycode+modifier pairs
 *       count              Actual number of steps used
 *
 * Modifier bitmask (same as QMK):
 *   bit 0 = LCTL
 *   bit 1 = LSFT
 *   bit 2 = LALT
 *   bit 3 = LGUI
 *   bit 4 = RCTL
 *   bit 5 = RSFT
 *   bit 6 = RALT
 *   bit 7 = RGUI
 *
 * VIA Custom Commands:
 *   0x40  GET layer config  (one layer per request)
 *   0x41  SET layer config  (one layer per request)
 *   0x42  GET single key    (layer + key index)
 *   0x43  SET single key    (layer + key index)
 *   0x44  RESET all         (restore defaults)
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>


// ---------------------------------------------------------------------------
// Modifier bitmask constants
// ---------------------------------------------------------------------------

#define MACRO_MOD_LCTL  (1<<0)
#define MACRO_MOD_LSFT  (1<<1)
#define MACRO_MOD_LALT  (1<<2)
#define MACRO_MOD_LGUI  (1<<3)
#define MACRO_MOD_RCTL  (1<<4)
#define MACRO_MOD_RSFT  (1<<5)
#define MACRO_MOD_RALT  (1<<6)
#define MACRO_MOD_RGUI  (1<<7)

// ---------------------------------------------------------------------------
// Color profiles
// ---------------------------------------------------------------------------

#define COLOR_PROFILE_COUNT  5

typedef enum {
    PROFILE_OCEAN  = 0,  // cyan on dark blue
    PROFILE_EMBER  = 1,  // orange on dark charcoal
    PROFILE_FOREST = 2,  // green on dark green
    PROFILE_DUSK   = 3,  // magenta on dark purple
    PROFILE_MONO   = 4,  // white on black
} color_profile_id_t;

typedef struct {
    const char *name;
    uint16_t    accent;      // text / label color
    uint16_t    hdr_bg;      // header background
    uint16_t    hdr_text;    // header text color
    uint16_t    hdr_line;    // header bottom line
    uint16_t    key_bg0;     // key row even background
    uint16_t    key_bg1;     // key row odd background
    uint16_t    key_text;    // key label text
    uint16_t    div;         // divider color
} color_profile_t;

// Defined in dwin_render.c
extern const color_profile_t COLOR_PROFILES[COLOR_PROFILE_COUNT];

// ---------------------------------------------------------------------------
// Encoder mode
// ---------------------------------------------------------------------------
typedef enum {
    ENC_MODE_LAYER    = 0,
    ENC_MODE_VOLUME   = 1,
    ENC_MODE_MACRO    = 2,
    ENC_MODE_SETTINGS = 3,
    ENC_MODE_COUNT    = 4
} encoder_mode_t;

// ---------------------------------------------------------------------------
// Settings state
// ---------------------------------------------------------------------------

typedef enum {
    SETTINGS_BRIGHTNESS = 0,
    SETTINGS_PROFILE    = 1,
    SETTINGS_TIMEOUT    = 2,
    SETTINGS_BACKLIGHT  = 3,
    SETTINGS_OFF        = 4,
    SETTINGS_EXIT       = 5,
    SETTINGS_NUM        = 6,
    SETTINGS_NONE       = 7, // special setting for no focus
} settings_item_t;

typedef struct {
    settings_item_t focus;
    bool editing;
} settings_state_t;

typedef enum {
    DISPLAY_OFF = 0,
    DISPLAY_ON  = 1,
} display_state_t;

typedef enum {
    VIA_GET_SIZES = 0x3f,
    VIA_GET_LAYER_NAME,
    VIA_SET_LAYER_NAME,
    VIA_GET_KEY_LABEL,
    VIA_SET_KEY_LABEL,
    VIA_GET_KEY_MACRO,
    VIA_SET_KEY_MACRO,
    VIA_GET_ENC_KEY_LABEL,
    VIA_SET_ENC_KEY_LABEL,
    VIA_GET_ENC_KEY_MACRO,
    VIA_SET_ENC_KEY_MACRO,
    VIA_RESET_CONFIG,
    VIA_GET_ACTIVE,
    VIA_SET_ACTIVE,
    VIA_REDRAW_DISPLAY,
} via_commands_t;

// ---------------------------------------------------------------------------
// Structs
// ---------------------------------------------------------------------------
typedef struct __attribute__((packed)) {
    uint8_t mods;
    uint8_t keycode;
} macro_step_t;

typedef struct __attribute__((packed)) {
    uint8_t      count;
    macro_step_t steps[MACRO_STEPS];
} macro_t;

typedef struct __attribute__((packed)) {
    char    label[MACROPAD_LABEL_LEN];
    macro_t macro;
} key_config_t;

typedef struct __attribute__((packed)) {
    char         layer_name[MACROPAD_NAME_LEN];
    key_config_t keys[MACROPAD_NUM_KEYS];
} layer_config_t;

typedef union __attribute__((packed)) {
  uint32_t raw;
  struct {
    uint8_t encoder_mode  :2; // encoder_mode_t
    uint8_t cur_layer     :2; // layer index 0-3
    uint8_t color_profile :3; // color_profile_id_t
    uint8_t brightness    :8; // 0-255
    uint8_t display_state :1; // display_state_t
    uint8_t timeout       :6; // 0-60
  };
} active_config_t;

typedef struct __attribute__((packed)) {
    active_config_t active;
    layer_config_t  layers[MACROPAD_NUM_LAYERS];
    key_config_t    enc_macro[2];
} macropad_config_t;

_Static_assert(sizeof(macro_step_t     ) == MACRO_STEP_SIZE     , "MACRO_STEP_SIZE"     );
_Static_assert(sizeof(macro_t          ) == MACRO_SIZE          , "MACRO_SIZE"          );
_Static_assert(sizeof(key_config_t     ) == KEY_CONFIG_SIZE     , "KEY_CONFIG_SIZE"     );
_Static_assert(sizeof(layer_config_t   ) == LAYER_CONFIG_SIZE   , "LAYER_CONFIG_SIZE"   );
_Static_assert(sizeof(active_config_t  ) == ACTIVE_CONFIG_SIZE  , "ACTIVE_CONFIG_SIZE"  );
_Static_assert(sizeof(macropad_config_t) == MACROPAD_CONFIG_SIZE, "MACROPAD_CONFIG_SIZE");
// ---------------------------------------------------------------------------
// API
// ---------------------------------------------------------------------------

void macropad_config_init(void);

/**
 * Get config for a specific layer (read from RAM cache).
 * @param layer  Layer index 0-3
 * @return       Pointer to layer config, or NULL if out of range
 */
const layer_config_t    *macropad_config_get_layer(uint8_t layer);

/**
 * Get config for a specific key.
 * @param layer  Layer index 0-3
 * @param key    Key index 0-11
 * @return       Pointer to key config, or NULL if out of range
 */
const key_config_t      *macropad_config_get_key(uint8_t layer, uint8_t key);

/**
 * Get config for the active encoder mode.
 * @return       Value 0-4
 */
uint8_t                  macropad_config_get_encoder_mode(void);

/**
 * Get config for the encoder macros.
 * @return       Pointer to encoder macro array, or NULL if out of range
 */
const key_config_t      *macropad_config_get_encoder_macros(void);

/**
 * Get config for the current layer.
 * @return       Value 0-4
 */
uint8_t                  macropad_config_get_cur_layer(void);

/**
 * Get config for the active colorprofile.
 * @return       Value 0-4
 */
uint8_t                  macropad_config_get_color_profile(void);

/**
 * Get config for the available color profiles.
 * @return       Pointer to color profile array, or NULL if out of range
 */
const color_profile_t   *macropad_config_get_profile(void);

/**
 * Get config for the active brightness level.
 * @return       Value 0-255
 */
uint8_t                  macropad_config_get_brightness(void);

/**
 * Get config for the active timeout value.
 * @return       Value 0-60
 */
uint8_t                  macropad_config_get_timeout(void);

/**
 * Get config for the display state.
 * @return       Value 0-255
 */
uint8_t                  macropad_config_get_display_state(void);

/**
 * Write encoder mode to EEPROM.
 */
void macropad_config_set_encoder_mode(uint8_t mode);

/**
 * Write current layer to EEPROM.
 */
void macropad_config_set_cur_layer(uint8_t layer);

/**
 * Write current color profile to EEPROM.
 */
void macropad_config_set_color_profile(uint8_t profile_id);

/**
 * Write current brightness to EEPROM.
 */
void macropad_config_set_brightness(uint8_t brightness);

/**
 * Write current brightness to EEPROM.
 */
void macropad_config_set_timeout(uint8_t timeout);

/**
 * Write current display state to EEPROM.
 */
void macropad_config_set_display_state(uint8_t state);

/**
 * Reset all layers to defaults and write to EEPROM.
 */
void macropad_config_reset(void);

/**
 * Execute the macro for a given layer + key.
 * Sends all steps as keydown+keyup events.
 */
void macropad_config_exec_macro(uint8_t layer, uint8_t key);

/**
 * Execute the macro for the encoder.
 * Sends all steps as keydown+keyup events.
 */
void macropad_config_exec_enc_macro(bool clockwise);

/**
 * VIA custom command handler.
 * Call from via_custom_value_command_kb().
 */
void macropad_config_via_command(uint8_t *data, uint8_t length);
