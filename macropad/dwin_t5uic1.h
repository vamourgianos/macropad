/**
 * dwin_t5uic1.h — DWIN T5UIC1 Display Driver for QMK (ChibiOS / STM32)
 * ==============================================================
 * Protocol: AA [cmd] [data...] CC 33 C3 3C
 *
 * Wiring (STM32F103C8T6):
 *   DWIN VCC  -> 5V
 *   DWIN GND  -> GND
 *   DWIN RX   -> USART TX pin (e.g. PA9  for USART1)
 *   DWIN TX   -> USART RX pin (e.g. PA10 for USART1)
 *
 * config.h additions needed:
 *   #define DWIN_UART_DRIVER   SD1       // ChibiOS serial driver
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ---------------------------------------------------------------------------
// Configuration defaults (override in config.h)
// ---------------------------------------------------------------------------

#ifndef DWIN_UART_DRIVER
#    define DWIN_UART_DRIVER SD1
#endif

#ifndef DWIN_CMD_DELAY_MS
#    define DWIN_CMD_DELAY_MS 20
#endif
#ifndef DWIN_CMD_DELAY_SMALL_MS
#    define DWIN_CMD_DELAY_SMALL_MS 20
#endif

#ifndef DWIN_HANDSHAKE_TIMEOUT_MS
#    define DWIN_HANDSHAKE_TIMEOUT_MS 500
#endif

// ---------------------------------------------------------------------------
// RGB565 color definitions
// ---------------------------------------------------------------------------

#define DWIN_BLACK    0x0000
#define DWIN_WHITE    0xFFFF
#define DWIN_RED      0xF800
#define DWIN_GREEN    0x07E0
#define DWIN_BLUE     0x001F
#define DWIN_CYAN     0x07FF
#define DWIN_MAGENTA  0xF81F
#define DWIN_YELLOW   0xFFE0
#define DWIN_ORANGE   0xFD20
#define DWIN_DGRAY    0x2104
#define DWIN_LGRAY    0xC618
#define DWIN_DBLUE    0x0A4C
#define DWIN_NAVY     0x000F

/** Convert 8-bit R,G,B to RGB565 */
#define DWIN_RGB(r, g, b) \
    ((uint16_t)(((r) & 0xF8) << 8) | (((g) & 0xFC) << 3) | ((b) >> 3))

// ---------------------------------------------------------------------------
// Screen orientation
// ---------------------------------------------------------------------------

typedef enum {
    DWIN_DIR_LANDSCAPE_0   = 0,
    DWIN_DIR_PORTRAIT_90   = 1,
    DWIN_DIR_LANDSCAPE_180 = 2,
    DWIN_DIR_PORTRAIT_270  = 3,
} dwin_direction_t;

// ---------------------------------------------------------------------------
// Font sizes
// ---------------------------------------------------------------------------

typedef enum {
    DWIN_FONT_6X12  = 0x00,
    DWIN_FONT_8X16  = 0x01,
    DWIN_FONT_10X20 = 0x02,
    DWIN_FONT_12X24 = 0x03,
    DWIN_FONT_14X28 = 0x04,
    DWIN_FONT_16X32 = 0x05,
    DWIN_FONT_20X40 = 0x06,
    DWIN_FONT_24X48 = 0x07,
    DWIN_FONT_28X56 = 0x08,
    DWIN_FONT_32X64 = 0x09,
} dwin_font_t;

// ---------------------------------------------------------------------------
// Rectangle modes
// ---------------------------------------------------------------------------

typedef enum {
    DWIN_RECT_FRAME = 0x00,  ///< Outline only
    DWIN_RECT_FILL  = 0x01,  ///< Solid fill
    DWIN_RECT_XOR   = 0x02,  ///< XOR (useful for selection)
} dwin_rect_mode_t;

// ---------------------------------------------------------------------------
// Move modes and directions
// ---------------------------------------------------------------------------

typedef enum {
    DWIN_MOVE_CIRCULAR  = 0x00,
    DWIN_MOVE_TRANSLATE = 0x80,
} dwin_move_mode_t;

typedef enum {
    DWIN_MOVE_LEFT  = 0x00,
    DWIN_MOVE_RIGHT = 0x01,
    DWIN_MOVE_UP    = 0x02,
    DWIN_MOVE_DOWN  = 0x03,
} dwin_move_dir_t;

// ---------------------------------------------------------------------------
// Memory types
// ---------------------------------------------------------------------------

typedef enum {
    DWIN_MEM_SRAM  = 0x5A,  ///< 32KB volatile SRAM
    DWIN_MEM_FLASH = 0xA5,  ///< 16KB non-volatile Flash
} dwin_mem_type_t;

// ===========================================================================
// (1) Initialisation
// ===========================================================================

/**
 * Initialise UART peripheral and send handshake.
 * Call once at keyboard startup (e.g. keyboard_post_init_user).
 *
 * @param direction  Screen orientation (use DWIN_DIR_LANDSCAPE for 480x272)
 * @param bg_color   Initial background color
 * @return           true if display acknowledged handshake
 */
bool dwin_init(dwin_direction_t direction, uint16_t bg_color);

/**
 * Send handshake packet and check response.
 * TX: AA 00 CC 33 C3 3C
 * RX: AA 00 4F 4B CC 33 C3 3C
 *
 * @return true if display responds with OK
 */
bool dwin_handshake(void);

// ===========================================================================
// (2) Configuration commands
// ===========================================================================

/**
 * Set backlight brightness.
 * @param brightness  0x00=off, 0xFF=full. Values 0x01-0x1F may flicker.
 */
void dwin_set_backlight(uint8_t brightness);

/**
 * Set screen orientation.
 * @param direction  dwin_direction_t value
 */
void dwin_set_direction(dwin_direction_t direction);

/**
 * Write bytes to SRAM or Flash memory.
 * Flash writes take up to 1 second. Flash has 100K write cycle limit.
 *
 * @param mem_type  DWIN_MEM_SRAM or DWIN_MEM_FLASH
 * @param address   Start address (SRAM: 0x0000-0x7FFF, Flash: 0x0000-0x3FFF)
 * @param data      Pointer to data buffer
 * @param length    Number of bytes to write
 */
void dwin_write_memory(dwin_mem_type_t mem_type, uint16_t address,
                       const uint8_t *data, size_t length);

/**
 * Read bytes from SRAM or Flash memory.
 *
 * @param mem_type  DWIN_MEM_SRAM or DWIN_MEM_FLASH
 * @param address   Start address
 * @param length    Number of bytes to read (max 0xF0)
 * @param out_buf   Buffer to store response (must be >= length bytes)
 * @return          Number of bytes actually read
 */
size_t dwin_read_memory(dwin_mem_type_t mem_type, uint16_t address,
                        uint8_t length, uint8_t *out_buf);

/**
 * Write SRAM contents into a JPEG picture memory slot.
 * @param pic_id  Slot 0x00-0x0F (each slot is 32KB)
 */
void dwin_write_picture_memory(uint8_t pic_id);

/**
 * Flush display buffer to screen. Call after drawing commands.
 */
void dwin_update(void);

/**
 * Send data out through the display's extended serial port.
 * @param data    Pointer to data
 * @param length  Number of bytes
 */
void dwin_serial_send(const uint8_t *data, size_t length);

// ===========================================================================
// (3) Drawing commands
// ===========================================================================

/**
 * Clear entire screen with a solid color (~1.5ms).
 * @param color  RGB565 fill color
 */
void dwin_clear(uint16_t color);

/**
 * Draw one or more points (nx*ny pixel blocks).
 *
 * @param color   RGB565 color
 * @param nx      Pixel width per point (1-15)
 * @param ny      Pixel height per point (1-15)
 * @param xs      Array of x coordinates
 * @param ys      Array of y coordinates
 * @param count   Number of points
 */
void dwin_draw_points(uint16_t color, uint8_t nx, uint8_t ny,
                      const uint16_t *xs, const uint16_t *ys, size_t count);

/**
 * Draw a single pixel (1x1 point).
 * @param color  RGB565 color
 * @param x, y  Coordinates
 */
void dwin_draw_pixel(uint16_t color, uint16_t x, uint16_t y);

/**
 * Draw a line between two points.
 * @param color      RGB565 color
 * @param x1,y1      Start point
 * @param x2,y2      End point
 */
void dwin_draw_line(uint16_t color,
                    uint16_t x1, uint16_t y1,
                    uint16_t x2, uint16_t y2);

/**
 * Draw multiple lines between two point vectors.
 * @param color      RGB565 color
 * @param xs, ys     Coordinate vectors
 */
void dwin_draw_polyline(uint16_t color,
                        const uint16_t *xs, const uint16_t *ys,
                        uint8_t count);

/**
 * Draw a horizontal line.
 * @param color   RGB565 color
 * @param x,y     Start point
 * @param length  Length in pixels
 */
void dwin_draw_hline(uint16_t color, uint16_t x, uint16_t y, uint16_t length);

/**
 * Draw a vertical line.
 * @param color   RGB565 color
 * @param x,y     Start point
 * @param length  Length in pixels
 */
void dwin_draw_vline(uint16_t color, uint16_t x, uint16_t y, uint16_t length);

/**
 * Draw a rectangle.
 * @param mode       DWIN_RECT_FRAME, DWIN_RECT_FILL, or DWIN_RECT_XOR
 * @param color      RGB565 color
 * @param x1,y1      Upper-left corner
 * @param x2,y2      Lower-right corner
 */
void dwin_draw_rectangle(dwin_rect_mode_t mode, uint16_t color,
                          uint16_t x1, uint16_t y1,
                          uint16_t x2, uint16_t y2);

/**
 * Scroll or translate a rectangular screen area.
 * @param mode        DWIN_MOVE_CIRCULAR or DWIN_MOVE_TRANSLATE
 * @param direction   DWIN_MOVE_LEFT/RIGHT/UP/DOWN
 * @param distance    Pixels to move
 * @param fill_color  Fill color for vacated area (TRANSLATE mode only)
 * @param x1,y1       Upper-left of area
 * @param x2,y2       Lower-right of area
 */
void dwin_move_area(dwin_move_mode_t mode, dwin_move_dir_t direction,
                    uint16_t distance, uint16_t fill_color,
                    uint16_t x1, uint16_t y1,
                    uint16_t x2, uint16_t y2);

// ===========================================================================
// (4) Text commands
// ===========================================================================

/**
 * Draw a null-terminated ASCII string.
 *
 * @param text         Null-terminated ASCII string
 * @param x, y         Upper-left coordinate
 * @param font         dwin_font_t size
 * @param color        Text color (RGB565)
 * @param bcolor       Background color (RGB565)
 * @param width_adjust Auto-adjust character width
 * @param show_bg      Display background color behind text
 */
void dwin_draw_string(const char *text,
                      uint16_t x, uint16_t y,
                      dwin_font_t font,
                      uint16_t color,
                      uint16_t bcolor,
                      bool width_adjust,
                      bool show_bg);

/**
 * Draw a numeric value (integer or fixed-point decimal).
 *
 * @param value             Integer value (scaled by 10^dec_digits)
 * @param x, y              Upper-left coordinate
 * @param int_digits        Integer digit count (1-20)
 * @param dec_digits        Decimal digit count (0-20, int+dec <= 20)
 * @param font              dwin_font_t size
 * @param color             Text color (RGB565)
 * @param bcolor            Background color (RGB565)
 * @param is_signed         True = signed number
 * @param zero_fill         Pad with leading zeros
 * @param show_leading_zero Show leading zeros as '0' (vs space)
 * @param show_bg           Display background color
 */
void dwin_draw_number(int32_t value,
                      uint16_t x, uint16_t y,
                      uint8_t int_digits, uint8_t dec_digits,
                      dwin_font_t font,
                      uint16_t color, uint16_t bcolor,
                      bool is_signed,
                      bool zero_fill,
                      bool show_leading_zero,
                      bool show_bg);

// ===========================================================================
// (5) Picture and icon commands
// ===========================================================================

/**
 * Display a QR code.
 * Final size = (46 * pixel_size) x (46 * pixel_size) pixels.
 *
 * @param data        Null-terminated string to encode (max 154 bytes)
 * @param x, y        Upper-left coordinate
 * @param pixel_size  Size of each QR dot in pixels (1-15)
 */
void dwin_show_qrcode(const char *data,
                      uint16_t x, uint16_t y,
                      uint8_t pixel_size);

/**
 * Display a JPEG image from picture memory (~250ms for 480x272).
 * Also caches in virtual display area #0.
 *
 * @param pic_id  Picture slot 0x00-0x0F
 */
void dwin_show_jpeg(uint8_t pic_id);

/**
 * Display an icon from the icon library in picture memory.
 *
 * @param lib_id          Icon library slot (0x00-0x0F)
 * @param icon_id         Icon index (0x00-0xFF)
 * @param x, y            Upper-left coordinate
 * @param show_bg         true=show background, false=filter black background
 * @param restore_bg      Auto-restore background from virtual area #0
 * @param filter_strength 0=normal, 1=enhanced (show_bg=false only)
 */
void dwin_show_icon(uint8_t lib_id, uint8_t icon_id,
                    uint16_t x, uint16_t y,
                    bool show_bg, bool restore_bg,
                    uint8_t filter_strength);

/**
 * Display an icon stored in SRAM.
 *
 * @param address         SRAM start address (0x0000-0x7FFF)
 * @param x, y            Upper-left coordinate
 * @param show_bg         true=show background, false=filter black
 * @param filter_strength 0=normal, 1=enhanced
 */
void dwin_show_icon_from_sram(uint16_t address,
                               uint16_t x, uint16_t y,
                               bool show_bg,
                               uint8_t filter_strength);

/**
 * Decompress a JPEG from picture memory into virtual display area #1.
 * @param pic_id  Picture slot 0x00-0x0F
 */
void dwin_decompress_jpeg_to_virtual(uint8_t pic_id);

/**
 * Copy/paste a region from a virtual display area to the screen.
 *
 * @param src_x1,src_y1   Upper-left of source in virtual area
 * @param src_x2,src_y2   Lower-right of source in virtual area
 * @param dst_x,dst_y     Destination upper-left on screen
 * @param virtual_area    0=#0, 1=#1
 * @param show_bg         true=show background
 * @param restore_bg      Auto-restore from area #0
 * @param filter_strength 0=normal, 1=enhanced
 */
void dwin_copy_from_virtual_area(uint16_t src_x1, uint16_t src_y1,
                                  uint16_t src_x2, uint16_t src_y2,
                                  uint16_t dst_x,  uint16_t dst_y,
                                  uint8_t virtual_area,
                                  bool show_bg, bool restore_bg,
                                  uint8_t filter_strength);

/**
 * Configure an icon animation group.
 *
 * @param anim_id             Animation group ID (0x00-0x0F)
 * @param enabled             true=on, false=off
 * @param lib_id              Icon library slot (0x00-0x0F)
 * @param icon_start          First icon ID
 * @param icon_end            Last icon ID
 * @param x, y                Display position (upper-left)
 * @param interval_10ms       Frame interval in units of 10ms
 * @param start_from_beginning true=restart, false=resume
 */
void dwin_set_icon_animation(uint8_t anim_id, bool enabled,
                              uint8_t lib_id,
                              uint8_t icon_start, uint8_t icon_end,
                              uint16_t x, uint16_t y,
                              uint8_t interval_10ms,
                              bool start_from_beginning);

/**
 * Enable/disable animation groups by bitmask.
 * Bit N controls animation group N (1=on, 0=off).
 * Example: dwin_control_animations(0x0003) enables groups 0 and 1.
 *
 * @param states  16-bit enable mask
 */
void dwin_control_animations(uint16_t states);

// ===========================================================================
// (6) Composite helpers
// ===========================================================================

/**
 * Draw a horizontal progress bar.
 *
 * @param x, y     Upper-left corner
 * @param width    Total bar width in pixels
 * @param height   Bar height in pixels
 * @param percent  Fill 0-100
 * @param fg_color Filled color
 * @param bg_color Empty color
 */
void dwin_draw_progress_bar(uint16_t x, uint16_t y,
                             uint16_t width, uint16_t height,
                             uint8_t percent,
                             uint16_t fg_color, uint16_t bg_color);

/**
 * Fill entire screen with color and update.
 * @param color  RGB565 color
 */
void dwin_fill_screen(uint16_t color);
