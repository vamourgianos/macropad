/**
 * dwin_render.h — Layer display renderer for DWIN T5UIC1
 * =======================================================
 *
 * Normal mode layout (480x272):
 *
 *  +------------------------------------------+
 *  |  LAYER NAME                  [MODE ▶]   |  y=0..44  header
 *  +------------+------------+----------------+
 *  |   Key 1    |   Key 2    |   Key 3        |  4 rows of keys
 *  |   Key 4    |   Key 5    |   Key 6        |
 *  |   Key 7    |   Key 8    |   Key 9        |
 *  |   Key 10   |   Key 11   |   Key 12       |
 *  +------------------------------------------+
 *
 * Settings mode layout:
 *
 *  +------------------------------------------+
 *  |  LAYER NAME              [SETTINGS ▶]   |  header
 *  +------------------------------------------+
 *  |                                          |
 *  |  ▶ Brightness                            |  bold = focused
 *  |    [████████████░░░░░░░░░░░░░░]  75%    |
 *  |                                          |
 *  |    Profile                               |
 *  |    LAYER 0                               |
 *  |                                          |
 *  +------------------------------------------+
 *
 *  Encoder in settings mode:
 *    Not editing: CW/CCW moves focus (▶) between items
 *                 Click enters edit mode (item highlighted)
 *    Editing:     CW/CCW changes value
 *                 Click exits edit mode
 */

#pragma once
#include "macropad_config.h"
#include <stdint.h>

/**
 * Initialise renderer. Call after dwin_init() and macropad_config_init().
 */
void dwin_render_init(void);

/**
 * Render the current layer to the display.
 * Call when layer changes.
 * @param layer  Current active layer (0-3)
 */
void dwin_render_layer(uint8_t layer);

/**
 * Change the current layer (faster than full render).
 * Call when layer changes.
 * @param layer  Current active layer (0-3)
 */
void dwin_change_layer(uint8_t layer);

/**
 * Update only the header mode badge.
 * Faster than full re-render when only mode changes.
 */
void dwin_render_mode_badge(encoder_mode_t mode);

/**
 * Update the brightness value.
 * Call when brightness value changes.
 * @param state    Current settings navigation state
 * @param brightness  Current brightness value 0-255
 */
void dwin_update_brightness(const settings_state_t *state, uint8_t brightness);

/**
 * Update the timeout value.
 * Call when timeout value changes.
 * @param state    Current settings navigation state
 * @param timeout     Current timeout value 0-60
 */
void dwin_update_timeout(const settings_state_t *state, uint8_t timeout);

/**
 * Update the backlight value.
 * Call when backlight value changes.
 * @param state    Current settings navigation state
 * @param timeout     Current backlight value 0-BACKLIGHT_LEVELS
 */
void dwin_update_backlight(const settings_state_t *state);

/**
 * Render the settings screen.
 * Call when entering settings mode or when settings state changes.
 * @param state    Current settings navigation state
 * @param brightness  Current brightness value 0-255
 * @param profile     Current profile (layer index 0-3)
 * @param timeout     Current timeout value 0-60
 */
void dwin_render_settings(const settings_state_t *state,
                          uint8_t brightness,
                          uint8_t profile,
                          uint8_t timeout);
