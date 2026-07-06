/**
 * dwin_render.c — Layer display renderer for DWIN T5UIC1
 * =======================================================
 * See dwin_render.h for layout documentation.
 */

#include "dwin_render.h"
#include "dwin_t5uic1.h"
#include "backlight.h"
#include "util.h"
#include <stdio.h>
#include <string.h>

// ---------------------------------------------------------------------------
// Color profile definitions
// ---------------------------------------------------------------------------

const color_profile_t COLOR_PROFILES[COLOR_PROFILE_COUNT] = {
    [PROFILE_OCEAN] = {
        .name      = "OCEAN",
        .accent    = DWIN_CYAN,
        .hdr_bg    = DWIN_DBLUE,
        .hdr_text  = DWIN_CYAN,
        .hdr_line  = DWIN_RGB(0, 180, 160),
        .key_bg0   = DWIN_BLACK,
        .key_bg1   = DWIN_RGB(10, 10, 30),
        .key_text  = DWIN_WHITE,
        .div       = DWIN_DGRAY,
    },
    [PROFILE_EMBER] = {
        .name      = "EMBER",
        .accent    = DWIN_ORANGE,
        .hdr_bg    = DWIN_RGB(40, 15, 0),
        .hdr_text  = DWIN_ORANGE,
        .hdr_line  = DWIN_RGB(180, 80, 0),
        .key_bg0   = DWIN_BLACK,
        .key_bg1   = DWIN_RGB(25, 10, 0),
        .key_text  = DWIN_RGB(255, 200, 150),
        .div       = DWIN_RGB(60, 30, 0),
    },
    [PROFILE_FOREST] = {
        .name      = "FOREST",
        .accent    = DWIN_GREEN,
        .hdr_bg    = DWIN_RGB(0, 30, 10),
        .hdr_text  = DWIN_GREEN,
        .hdr_line  = DWIN_RGB(0, 160, 60),
        .key_bg0   = DWIN_BLACK,
        .key_bg1   = DWIN_RGB(0, 20, 5),
        .key_text  = DWIN_RGB(150, 255, 150),
        .div       = DWIN_RGB(0, 50, 20),
    },
    [PROFILE_DUSK] = {
        .name      = "DUSK",
        .accent    = DWIN_MAGENTA,
        .hdr_bg    = DWIN_RGB(30, 0, 40),
        .hdr_text  = DWIN_MAGENTA,
        .hdr_line  = DWIN_RGB(160, 0, 180),
        .key_bg0   = DWIN_BLACK,
        .key_bg1   = DWIN_RGB(20, 0, 25),
        .key_text  = DWIN_RGB(255, 150, 255),
        .div       = DWIN_RGB(60, 0, 70),
    },
    [PROFILE_MONO] = {
        .name      = "MONO",
        .accent    = DWIN_WHITE,
        .hdr_bg    = DWIN_RGB(30, 30, 30),
        .hdr_text  = DWIN_WHITE,
        .hdr_line  = DWIN_LGRAY,
        .key_bg0   = DWIN_BLACK,
        .key_bg1   = DWIN_RGB(15, 15, 15),
        .key_text  = DWIN_LGRAY,
        .div       = DWIN_RGB(40, 40, 40),
    },
};

// ---------------------------------------------------------------------------
// Layout constants (480x272)
// ---------------------------------------------------------------------------

#define W    480
#define H    272

// Header
#define HDR_H       44
#define HDR_TEXT_Y   8

// Mode badge
#define BADGE_FONT   DWIN_FONT_8X16
#define BADGE_FH     10

// Key grid — 5 cols x 4 rows
#define GRID_COLS     MACROPAD_NUM_COLUMNS
#define GRID_ROWS     MACROPAD_NUM_ROWS
#define COL_W         (W / GRID_COLS)
#define ROW_H         ((H - HDR_H) / GRID_ROWS)
#define CHARS_PER_COL (COL_W / 10)
#define MAX_LABEL_IN_COL MIN(CHARS_PER_COL, MACROPAD_LABEL_LEN)
#define ROW_BUF_LEN   (W / 10)
#define COL_X(c)      ((c) * COL_W)
#define ROW_Y(r)      (HDR_H + (r) * ROW_H)
#define CELL_TEXT_DY  13

// Settings
#define SET_ITEM_H    ((H - HDR_H) / 7)
#define SET_ITEM_Y(n) (HDR_H + 10 + _setting_line[(n)] * SET_ITEM_H)
#define SET_ARROW_X   16
#define SET_LABEL_X   40
#define SET_VALUE_X   60
#define SET_BAR_X     60
#define SET_BAR_W     280
#define SET_BAR_H     18
#define SET_BAR_Y_OFF 28
// Color swatch dimensions
#define SWATCH_W      30
#define SWATCH_H      22
#define SWATCH_GAP     6

// Fonts
#define FONT_TITLE   DWIN_FONT_16X32
#define FONT_LABEL   DWIN_FONT_10X20
#define FONT_BADGE   DWIN_FONT_8X16
#define FONT_SET     DWIN_FONT_10X20

#define PAGE_NUM 2
// Mode label strings
static const char *_mode_labels[ENC_MODE_COUNT] = {
    "LAYER", "VOLUME", "MACRO", "SETTINGS"
};
static const uint8_t _setting_line[SETTINGS_NUM] = {
    0, 2, 5, 0, 2, 4
};
static const bool _setting_selection[SETTINGS_NUM][SETTINGS_NUM] = {
    {true , true , true , false, false, false},
    {true , true , true , false, false, false},
    {true , true , true , false, false, false},
    {false, false, false, true , true , true },
    {false, false, false, true , true , true },
    {false, false, false, true , true , true },
};
static const char* _setting_page[SETTINGS_NUM] = {
    "Page 1/2", "Page 1/2", "Page 1/2", "Page 2/2", "Page 2/2", "Page 2/2",
};
uint8_t               _prev_focus;
extern uint8_t        _cur_layer;
extern encoder_mode_t _enc_mode;

void get_actions(char *buf, size_t length, encoder_mode_t mode) {
    switch (mode) {
        case ENC_MODE_LAYER:
            snprintf(buf, length, "< Next | Prev >");
            return;
        case ENC_MODE_VOLUME:
            snprintf(buf, length, "< Vol- | Vol+ >");
            return;
        case ENC_MODE_MACRO: 
            const key_config_t *macros = macropad_config_get_encoder_macros();
            snprintf(buf, length, "< %s | %s >", macros[0].label, macros[1].label);
            return;
        case ENC_MODE_SETTINGS:
            buf[0] = '\0';
            return;
        default:
            snprintf(buf, length, "< Unknown >");
            return;
    }
}

// ---------------------------------------------------------------------------
// Internal: draw header
// ---------------------------------------------------------------------------
static void _draw_layer_name(uint8_t layer, const color_profile_t *p) {
    // Layer name
    const layer_config_t *cfg = macropad_config_get_layer(layer);
    if (cfg && cfg->layer_name[0]) {
        char name[MACROPAD_NAME_LEN + 1];
        strncpy(name, cfg->layer_name, MACROPAD_NAME_LEN);
        name[MACROPAD_NAME_LEN] = '\0';
        dwin_draw_rectangle(DWIN_RECT_FILL, p->hdr_bg, 0, 0, MACROPAD_NAME_LEN*10, HDR_H - 1);
        dwin_draw_string(name, 10, HDR_TEXT_Y,
                         FONT_TITLE, p->hdr_text, p->hdr_bg, false, true);
    }
}

static void _draw_header(uint8_t layer, encoder_mode_t mode) {
    const color_profile_t *p = macropad_config_get_profile();

    dwin_draw_rectangle(DWIN_RECT_FILL, p->hdr_bg, MACROPAD_NAME_LEN*10, 0, W, HDR_H - 1);
    // dwin_draw_hline(p->hdr_line, 0, HDR_H, W);

    _draw_layer_name(layer, p);

    // Mode badge right-aligned
    char badge[20];
    snprintf(badge, sizeof(badge), "[%s]", _mode_labels[(uint8_t)mode]);
    // uint8_t  len    = strnlen(badge, sizeof(badge));
    // uint16_t text_w = len * 8;
    uint16_t tx     = W - 100;
    uint16_t ty     = (HDR_H - BADGE_FH) / 4;

    char actions[40];
    get_actions(actions, sizeof(actions), mode);
    uint8_t  alen    = strnlen(actions, sizeof(actions));
    uint16_t atext_w = alen * 8;
    uint16_t atx     = W - atext_w - 8;
    uint16_t aty     = 3 * ((HDR_H - BADGE_FH) / 4);
    // Clear badge area
    dwin_draw_rectangle(DWIN_RECT_FILL, p->hdr_bg,
                        W - 140, 0, W, HDR_H - 1);
    dwin_draw_string(badge, tx, ty,
                     FONT_BADGE, p->accent, p->hdr_bg, false, true);
    dwin_draw_string(actions, atx, aty,
                     FONT_BADGE, p->accent, p->hdr_bg, false, true);
}

// ---------------------------------------------------------------------------
// Internal: draw key grid
// ---------------------------------------------------------------------------

static void _draw_key_labels(uint8_t layer, const color_profile_t *p, const layer_config_t  *cfg) {
    for (uint8_t row = 0; row < GRID_ROWS; row++) {
        uint16_t bg = (row % 2 == 0) ? p->key_bg0 : p->key_bg1;
        uint16_t y1 = ROW_Y(row);
        uint16_t y2 = y1 + ROW_H - 1;

        dwin_draw_rectangle(DWIN_RECT_FILL, bg, 0, y1, W, y2);

        // for (uint8_t col = 0; col < GRID_COLS; col++) {
        //     uint8_t  idx = row * GRID_COLS + col;
        //     uint16_t x1  = COL_X(col);
        //     // uint16_t x2  = x1 + COL_W - 1;


        //     if (cfg->keys[idx].label[0]) {
        //         char label[MACROPAD_LABEL_LEN + 1];
        //         strncpy(label, cfg->keys[idx].label, MACROPAD_LABEL_LEN+1);
        //         label[MACROPAD_LABEL_LEN] = '\0';
        //         // try to center the label, but left-align if it doesn't fit
        //         uint8_t  llen   = strnlen(label, MACROPAD_LABEL_LEN+1);
        //         uint16_t text_w = llen * 10;
        //         uint16_t tx     = x1 + (COL_W > text_w ?
        //                                 (COL_W - text_w) / 2 : 2);
        //         uint16_t ty     = y1 + CELL_TEXT_DY;
        //         dwin_draw_string(label, tx, ty,
        //                          FONT_LABEL, p->key_text, bg, false, true);
        //     }
        // }
        char combined[ROW_BUF_LEN + 1];
        memset(combined, ' ', ROW_BUF_LEN);
        combined[ROW_BUF_LEN] = '\0';

        for (uint8_t col = 0; col < GRID_COLS; col++) {
            uint8_t idx = row * GRID_COLS + col;
            if (cfg->keys[idx].label[0]) {
                uint8_t llen       = strnlen(cfg->keys[idx].label, MAX_LABEL_IN_COL);
                uint8_t center_off = (CHARS_PER_COL > llen) ? (CHARS_PER_COL - llen) / 2 : 0;
                uint8_t pos        = COL_X(col) / 10 + center_off;
                uint8_t copy_len   = MIN(llen, ROW_BUF_LEN - pos);
                memcpy(combined + pos, cfg->keys[idx].label, copy_len);
            }
        }
        dwin_draw_string(combined, COL_X(0), y1 + CELL_TEXT_DY, FONT_LABEL, p->key_text, bg, false, true);
    }
}

static void _draw_key_grid(uint8_t layer) {
    const color_profile_t *p   = macropad_config_get_profile();
    const layer_config_t  *cfg = macropad_config_get_layer(layer);
    if (!cfg) return;

    _draw_key_labels(layer, p, cfg);
    // (GRID_COLS+1)*2 vertical + 1 return + (GRID_ROWS-1)*2 horizontal + GRID_ROWS connectors
    uint16_t xs[(GRID_COLS + 1) * 2 + 1 + (GRID_ROWS - 1) * 2 + GRID_ROWS];
    uint16_t ys[(GRID_COLS + 1) * 2 + 1 + (GRID_ROWS - 1) * 2 + GRID_ROWS];
    uint8_t  n = 0;

    // --- Vertical snake through all column boundaries including edges ---
    for (uint8_t c = 0; c <= GRID_COLS; c++) {
        uint16_t x = COL_X(c);
        if (x == W) x = W-1;
        if (c % 2 == 0) {
            xs[n] = x; ys[n++] = HDR_H;
            xs[n] = x; ys[n++] = H-1;
        } else {
            xs[n] = x; ys[n++] = H-1;
            xs[n] = x; ys[n++] = HDR_H;
        }
    }

    // --- Return to left along top or bottom edge ---
    // GRID_COLS even → ended at H, GRID_COLS odd → ended at HDR_H
    uint16_t end_y = (GRID_COLS % 2 == 0) ? H-1 : HDR_H;
    xs[n] = 0; ys[n++] = end_y;

    // --- Horizontal snake through interior row dividers ---
    // Connectors between rows are vertical lines on left/right edge (not diagonal)
    bool going_right = true;
    if (end_y == HDR_H) {
        for (uint8_t r = 1; r <= GRID_ROWS; r++) {
            uint16_t y = ROW_Y(r) - 1;
            xs[n] = going_right ? 0 : W-1; ys[n++] = y;
            xs[n] = going_right ? W-1 : 0; ys[n++] = y;
            going_right = !going_right;
        }
    } else {
        for (int8_t r = GRID_ROWS; r >= 1; r--) {
            uint16_t y = ROW_Y(r) - 1;
            xs[n] = going_right ? 0 : W-1; ys[n++] = y;
            xs[n] = going_right ? W-1 : 0; ys[n++] = y;
            going_right = !going_right;
        }
    }

    dwin_draw_polyline(p->div, xs, ys, n);
}
// ---------------------------------------------------------------------------
// Internal: draw settings
// ---------------------------------------------------------------------------

static inline void _render_brightness(const settings_state_t *state, uint8_t brightness, bool render_header)
{
    const color_profile_t *p = macropad_config_get_profile();
    bool     is_focus = (state->focus == SETTINGS_BRIGHTNESS);
    bool     is_edit  = is_focus && state->editing;
    uint16_t y        = SET_ITEM_Y(SETTINGS_BRIGHTNESS);

    uint16_t label_col = is_edit ? p->accent : is_focus ? DWIN_WHITE : DWIN_LGRAY;
    if (render_header) {
        char *symbol = is_edit ? "*" : is_focus ? ">" : " ";
        dwin_draw_string(symbol, SET_ARROW_X, y, FONT_SET, p->accent, DWIN_BLACK, false, true);
        dwin_draw_string("Display brightness", SET_LABEL_X, y, FONT_SET, label_col, DWIN_BLACK, false, true);
    }
    // Bar
    uint16_t bar_y   = y + SET_BAR_Y_OFF;
    uint16_t fill_w  = (uint16_t)((uint32_t)SET_BAR_W * brightness / 255);
    dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_DGRAY, SET_BAR_X, bar_y, SET_BAR_X + SET_BAR_W, bar_y + SET_BAR_H);
    if (fill_w > 0) {
        dwin_draw_rectangle(DWIN_RECT_FILL, p->accent, SET_BAR_X, bar_y, SET_BAR_X + fill_w, bar_y + SET_BAR_H);
    }
    // Percentage
    char pct[8];
    snprintf(pct, sizeof(pct), "%3d%%", (brightness * 100) / 255);
    dwin_draw_string(pct, SET_BAR_X + SET_BAR_W + 12, bar_y, FONT_SET, label_col, DWIN_BLACK, false, true);

}

static inline void _render_profile(const settings_state_t *state, uint8_t brightness, uint8_t profile)
{
    const color_profile_t *p = macropad_config_get_profile();
    bool     is_focus = (state->focus == SETTINGS_PROFILE);
    bool     is_edit  = is_focus && state->editing;
    uint16_t y        = SET_ITEM_Y(SETTINGS_PROFILE);
    uint16_t label_col = is_edit ? p->accent : is_focus ? DWIN_WHITE : DWIN_LGRAY;
    char *symbol = is_edit ? "*" : is_focus ? ">" : " ";
    if (is_edit) {
        // Header
        _draw_header(_cur_layer, ENC_MODE_SETTINGS);

        // Body background
        dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_BLACK, 0, HDR_H, W, H);
        dwin_draw_string(_setting_page[SETTINGS_PROFILE], W - 100, (HDR_H + 10), FONT_SET, DWIN_WHITE, DWIN_BLACK, false, true);
        
        _render_brightness(state, brightness, true);
    }
    dwin_draw_string(symbol, SET_ARROW_X, y, FONT_SET, p->accent, DWIN_BLACK, false, true);
    dwin_draw_string("Profile", SET_LABEL_X, y, FONT_SET, label_col, DWIN_BLACK, false, true);

    uint16_t val_y = y + SET_BAR_Y_OFF;

    // Profile name
    if (profile < COLOR_PROFILE_COUNT) {
        const color_profile_t *sel = &COLOR_PROFILES[profile];
        dwin_draw_string(sel->name, SET_VALUE_X, val_y, FONT_SET, label_col, DWIN_BLACK, false, true);
        // Two color swatches:
        // Swatch 1 = accent color, Swatch 2 = header background
        uint16_t sw_x = SET_VALUE_X + 100;
        // Swatch 1: accent
        dwin_draw_rectangle(DWIN_RECT_FILL, sel->accent, sw_x, val_y, sw_x + SWATCH_W, val_y + SWATCH_H);
        dwin_draw_rectangle(DWIN_RECT_FRAME, DWIN_LGRAY, sw_x, val_y, sw_x + SWATCH_W, val_y + SWATCH_H);
        // Swatch 2: header background
        uint16_t sw2_x = sw_x + SWATCH_W + SWATCH_GAP;
        dwin_draw_rectangle(DWIN_RECT_FILL, sel->hdr_bg, sw2_x, val_y, sw2_x + SWATCH_W, val_y + SWATCH_H);
        dwin_draw_rectangle(DWIN_RECT_FRAME, DWIN_LGRAY, sw2_x, val_y, sw2_x + SWATCH_W, val_y + SWATCH_H);
        // Show all 5 profile swatches as small dots below
        uint16_t dots_x = SET_VALUE_X;
        uint16_t dots_y = val_y + SWATCH_H + 6;
        for (uint8_t pi = 0; pi < COLOR_PROFILE_COUNT; pi++) {
            uint16_t dot_x  = dots_x + pi * (14 + 4);
            uint16_t dot_col = COLOR_PROFILES[pi].accent;
            // Fill dot
            dwin_draw_rectangle(DWIN_RECT_FILL, dot_col, dot_x, dots_y, dot_x + 14, dots_y + 8);
            // Active indicator: white border
            if (pi == profile) {
                dwin_draw_rectangle(DWIN_RECT_FRAME, DWIN_WHITE, dot_x - 1, dots_y - 1, dot_x + 15, dots_y + 9);
            }
        }
    }
}

static inline void _render_timeout(const settings_state_t *state, uint8_t timeout, bool render_header)
{
    const color_profile_t *p = macropad_config_get_profile();
    bool     is_focus = (state->focus == SETTINGS_TIMEOUT);
    bool     is_edit  = is_focus && state->editing;
    uint16_t y        = SET_ITEM_Y(SETTINGS_TIMEOUT);
    uint16_t label_col = is_edit ? p->accent : is_focus ? DWIN_WHITE : DWIN_LGRAY;

    if (render_header) {
        char *symbol = is_edit ? "*" : is_focus ? ">" : " ";
        dwin_draw_string(symbol, SET_ARROW_X, y, FONT_SET, p->accent, DWIN_BLACK, false, true);
        dwin_draw_string("Sleep after:", SET_LABEL_X, y, FONT_SET, label_col, DWIN_BLACK, false, true);
    }

    // Bar
    uint16_t bar_y   = y + SET_BAR_Y_OFF;
    uint16_t fill_w  = (uint16_t)((uint32_t)SET_BAR_W * timeout / 60);
    dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_DGRAY, SET_BAR_X, bar_y, SET_BAR_X + SET_BAR_W, bar_y + SET_BAR_H);
    if (fill_w > 0) {
        dwin_draw_rectangle(DWIN_RECT_FILL, p->accent, SET_BAR_X, bar_y, SET_BAR_X + fill_w, bar_y + SET_BAR_H);
    }
    // Value
    char pct[12];
    if (timeout == 0) {
        dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_BLACK, SET_BAR_X + SET_BAR_W + 12, bar_y, SET_BAR_X + SET_BAR_W + 120, bar_y + SET_BAR_H);
        snprintf(pct, sizeof(pct), "OFF");
    }
    else
        snprintf(pct, sizeof(pct), "%3d minutes", timeout);
    dwin_draw_string(pct, SET_BAR_X + SET_BAR_W + 12, bar_y, FONT_SET, label_col, DWIN_BLACK, false, true);
}

static inline void _render_backlight(const settings_state_t *state, bool render_header)
{
    const color_profile_t *p = macropad_config_get_profile();
    bool     is_focus = (state->focus == SETTINGS_BACKLIGHT);
    bool     is_edit  = is_focus && state->editing;
    uint16_t y        = SET_ITEM_Y(SETTINGS_BACKLIGHT);
    uint16_t label_col = is_edit ? p->accent : is_focus ? DWIN_WHITE : DWIN_LGRAY;

    if (render_header) {
        char *symbol = is_edit ? "*" : is_focus ? ">" : " ";
        dwin_draw_string(symbol, SET_ARROW_X, y, FONT_SET, p->accent, DWIN_BLACK, false, true);
        dwin_draw_string("Backlight brightness:", SET_LABEL_X, y, FONT_SET, label_col, DWIN_BLACK, false, true);
    }
    uint8_t value = get_backlight_level();
    // Bar
    uint16_t bar_y   = y + SET_BAR_Y_OFF;
    uint16_t fill_w  = (uint16_t)((uint32_t)SET_BAR_W * value / BACKLIGHT_LEVELS);
    dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_DGRAY, SET_BAR_X, bar_y, SET_BAR_X + SET_BAR_W, bar_y + SET_BAR_H);
    if (fill_w > 0) {
        dwin_draw_rectangle(DWIN_RECT_FILL, p->accent, SET_BAR_X, bar_y, SET_BAR_X + fill_w, bar_y + SET_BAR_H);
    }
    // Value
    char pct[12];
    if (value == 0) {
        dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_BLACK, SET_BAR_X + SET_BAR_W + 12, bar_y, SET_BAR_X + SET_BAR_W + 120, bar_y + SET_BAR_H);
        snprintf(pct, sizeof(pct), "OFF");
    }
    else
        snprintf(pct, sizeof(pct), "%2d/%2d", value, BACKLIGHT_LEVELS);
    dwin_draw_string(pct, SET_BAR_X + SET_BAR_W + 12, bar_y, FONT_SET, label_col, DWIN_BLACK, false, true);
}

static inline void _render_display_off(const settings_state_t *state)
{
    const color_profile_t *p = macropad_config_get_profile();
    bool     is_focus = (state->focus == SETTINGS_OFF);
    bool     is_edit  = is_focus && state->editing;
    uint16_t y        = SET_ITEM_Y(SETTINGS_OFF);

    uint16_t label_col = is_edit ? p->accent : is_focus ? DWIN_WHITE : DWIN_LGRAY;
    char *symbol = is_edit ? "*" : is_focus ? ">" : " ";
    dwin_draw_string(symbol, SET_ARROW_X, y, FONT_SET, p->accent, DWIN_BLACK, false, true);
    dwin_draw_string("Turn Display Off", SET_LABEL_X, y, FONT_SET, label_col, DWIN_BLACK, false, true);
}

static inline void _render_settings_exit(const settings_state_t *state)
{
    const color_profile_t *p = macropad_config_get_profile();
    bool     is_focus = (state->focus == SETTINGS_EXIT);
    bool     is_edit  = is_focus && state->editing;
    uint16_t y        = SET_ITEM_Y(SETTINGS_EXIT);

    uint16_t label_col = is_edit ? p->accent : is_focus ? DWIN_WHITE : DWIN_LGRAY;
    char *symbol = is_edit ? "*" : is_focus ? ">" : " ";
    dwin_draw_string(symbol, SET_ARROW_X, y, FONT_SET, p->accent, DWIN_BLACK, false, true);
    dwin_draw_string("Exit Settings Menu", SET_LABEL_X, y, FONT_SET, label_col, DWIN_BLACK, false, true);
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
void dwin_render_init(void) {
    dwin_clear(DWIN_BLACK);
    dwin_update();
}

void dwin_render_mode_badge(encoder_mode_t mode) {
    const color_profile_t *p = macropad_config_get_profile();
    // Mode badge right-aligned
    char badge[20];
    snprintf(badge, sizeof(badge), "[%s]", _mode_labels[(uint8_t)mode]);
    // uint8_t  len    = strnlen(badge, sizeof(badge));
    // uint16_t text_w = len * 8;
    uint16_t tx     = W - 100;
    uint16_t ty     = (HDR_H - BADGE_FH) / 4;

    char actions[40];
    get_actions(actions, sizeof(actions), mode);
    uint8_t  alen    = strnlen(actions, sizeof(actions));
    uint16_t atext_w = alen * 8;
    uint16_t atx     = W - atext_w - 8;
    uint16_t aty     = 3 * ((HDR_H - BADGE_FH) / 4);
    // Clear badge area
    dwin_draw_rectangle(DWIN_RECT_FILL, p->hdr_bg, W - 140, 0, W, HDR_H - 1);
    dwin_draw_string(badge, tx, ty,
                     FONT_BADGE, p->accent, p->hdr_bg, false, true);
    dwin_draw_string(actions, atx, aty,
                     FONT_BADGE, p->accent, p->hdr_bg, false, true);
    dwin_update();
}

void dwin_render_layer(uint8_t layer) {
    dwin_clear(DWIN_BLACK);
    _draw_header(layer, _enc_mode);
    _draw_key_grid(layer);
    dwin_update();
}

void dwin_change_layer(uint8_t layer) {
    // Layer name
    const color_profile_t *p = macropad_config_get_profile();

    _draw_layer_name(layer, p);
    _draw_key_grid(layer);
    dwin_update();
}

void dwin_update_brightness(const settings_state_t *state, uint8_t brightness)
{
    _render_brightness(state, brightness, false);
}

void dwin_update_timeout(const settings_state_t *state, uint8_t timeout)
{
    _render_timeout(state, timeout, false);
}

void dwin_update_backlight(const settings_state_t *state)
{
    _render_backlight(state, false);
}

void dwin_render_settings(const settings_state_t *state,
                          uint8_t brightness,
                          uint8_t profile,
                          uint8_t timeout)
{
    settings_item_t focus = state->focus;
    if (state->focus == SETTINGS_NONE) {
        focus = SETTINGS_BRIGHTNESS;
        _prev_focus = SETTINGS_BRIGHTNESS;
    }
    bool page_change = false;
    if ((_prev_focus == SETTINGS_TIMEOUT    && focus == SETTINGS_BACKLIGHT ) ||
        (_prev_focus == SETTINGS_EXIT       && focus == SETTINGS_BRIGHTNESS) ||
        (_prev_focus == SETTINGS_BRIGHTNESS && focus == SETTINGS_EXIT      ) ||
        (_prev_focus == SETTINGS_BACKLIGHT  && focus == SETTINGS_TIMEOUT   )   )
       page_change = true;
    if (page_change || (state->focus == SETTINGS_NONE)) {
        // Header
        _draw_header(_cur_layer, ENC_MODE_SETTINGS);

        // Body background
        dwin_draw_rectangle(DWIN_RECT_FILL, DWIN_BLACK, 0, HDR_H, W, H);
        dwin_draw_string(_setting_page[focus], W - 100, (HDR_H + 10), FONT_SET, DWIN_WHITE, DWIN_BLACK, false, true);
    }
    _prev_focus = focus;
    for (uint8_t i = 0; i < SETTINGS_NUM; i++) {
        switch (i) {
        case SETTINGS_BRIGHTNESS:
            if (_setting_selection[focus][i]) {
                _render_brightness(state, brightness, true);
            }
            break;
        case SETTINGS_PROFILE:
            if (_setting_selection[focus][i]) {
                _render_profile(state, brightness, profile);
            }
            break;
        case SETTINGS_TIMEOUT:
            if (_setting_selection[focus][i]) {
                _render_timeout(state, timeout, true);
            }
            break;
        case SETTINGS_BACKLIGHT:
            if (_setting_selection[focus][i]) {
                _render_backlight(state, true);
            }
            break;
        case SETTINGS_OFF:
            if (_setting_selection[focus][i]) {
                _render_display_off(state);
            }
            break;
        case SETTINGS_EXIT:
            if (_setting_selection[focus][i]) {
                _render_settings_exit(state);
            }
            break;
        }
    }

    dwin_update();
}
