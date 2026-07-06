/**
 * dwin_t5uic1.c — DWIN T5UIC1 Display Driver for QMK (ChibiOS / STM32)
 * ==============================================================
 */

#include "dwin_t5uic1.h"
#include "quantum.h"       // QMK core (wait_ms, etc.)

// ---------------------------------------------------------------------------
// Internal packet buffer and tail
// ---------------------------------------------------------------------------

#define DWIN_BUF_SIZE 256
#define DWIN_TAIL_LEN 4

static const uint8_t DWIN_TAIL[DWIN_TAIL_LEN] = {0xCC, 0x33, 0xC3, 0x3C};
static uint8_t       _buf[DWIN_BUF_SIZE];

// ---------------------------------------------------------------------------
// Internal UART helpers
// ---------------------------------------------------------------------------

/** ChibiOS serial driver config: 115200 8N1 */
static const SerialConfig dwin_serial_cfg = {
    .speed = 115200,
    .cr1   = 0,
    .cr2   = USART_CR2_STOP1_BITS,
    .cr3   = 0,
};

/** Send raw bytes directly to UART */
static inline void _uart_write(const uint8_t *data, size_t len) {
    sdWrite(&DWIN_UART_DRIVER, data, len);
}

/** Read up to `len` bytes from UART into buf, with timeout_ms */
static size_t _uart_read(uint8_t *buf, size_t len, uint32_t timeout_ms) {
    size_t received = 0;
    uint32_t deadline = timer_read32() + timeout_ms;
    while (received < len && timer_read32() < deadline) {
        msg_t c = sdGetTimeout(&DWIN_UART_DRIVER,
                               TIME_MS2I(10));
        if (c != MSG_TIMEOUT) {
            buf[received++] = (uint8_t)c;
        }
    }
    return received;
}

// ---------------------------------------------------------------------------
// Packet builder helpers
// ---------------------------------------------------------------------------

static inline void _w16(uint8_t *buf, size_t *i, uint16_t v) {
    buf[(*i)++] = (v >> 8) & 0xFF;
    buf[(*i)++] = v & 0xFF;
}

static inline void _w8(uint8_t *buf, size_t *i, uint8_t v) {
    buf[(*i)++] = v;
}

/**
 * Transmit a complete DWIN packet.
 * Prepends 0xAA and appends TAIL automatically.
 *
 * @param payload  Command byte + data (without 0xAA or tail)
 * @param len      Length of payload
 */
static void _send(const uint8_t *payload, size_t len) {
    // AA + payload + tail
    uint8_t header = 0xAA;
    _uart_write(&header, 1);
    _uart_write(payload, len);
    _uart_write(DWIN_TAIL, DWIN_TAIL_LEN);
    wait_ms(DWIN_CMD_DELAY_MS);
}

static void _send_fast(const uint8_t *payload, size_t len) {
    // AA + payload + tail
    uint8_t header = 0xAA;
    _uart_write(&header, 1);
    _uart_write(payload, len);
    _uart_write(DWIN_TAIL, DWIN_TAIL_LEN);
    wait_ms(DWIN_CMD_DELAY_SMALL_MS);  // 1ms is plenty for any line/rect command
}

// ===========================================================================
// (1) Initialisation
// ===========================================================================

bool dwin_init(dwin_direction_t direction, uint16_t bg_color) {
    sdStart(&DWIN_UART_DRIVER, &dwin_serial_cfg);
    wait_ms(500);

    bool ok = dwin_handshake();
    wait_ms(100);
    dwin_set_direction(direction);
    dwin_clear(bg_color);
    dwin_update();
    wait_ms(100);
    return ok;
}

bool dwin_handshake(void) {
    uint8_t pkt[] = {0x00};
    _send(pkt, 1);

    uint8_t resp[8] = {0};
    size_t  n = _uart_read(resp, sizeof(resp),
                           DWIN_HANDSHAKE_TIMEOUT_MS);

    return (n >= 4       &&
            resp[0] == 0xAA &&
            resp[1] == 0x00 &&
            resp[2] == 0x4F &&  // 'O'
            resp[3] == 0x4B);   // 'K'
}

// ===========================================================================
// (2) Configuration commands
// ===========================================================================

void dwin_set_backlight(uint8_t brightness) {
    // uint8_t brightness_real = 0xFF - brightness; // invert: 0=off, 255=full
    uint8_t pkt[] = {0x30, brightness};
    _send_fast(pkt, sizeof(pkt));
}

void dwin_set_direction(dwin_direction_t direction) {
    uint8_t pkt[] = {0x34, 0x5A, 0xA5, (uint8_t)(direction & 0x03)};
    _send_fast(pkt, sizeof(pkt));
}

void dwin_write_memory(dwin_mem_type_t mem_type, uint16_t address,
                       const uint8_t *data, size_t length) {
    size_t i = 0;
    _w8(_buf, &i, 0x31);
    _w8(_buf, &i, (uint8_t)mem_type);
    _w16(_buf, &i, address);
    for (size_t j = 0; j < length && i < DWIN_BUF_SIZE; j++) {
        _w8(_buf, &i, data[j]);
    }
    _send(_buf, i);
}

size_t dwin_read_memory(dwin_mem_type_t mem_type, uint16_t address,
                        uint8_t length, uint8_t *out_buf) {
    size_t i = 0;
    _w8(_buf, &i, 0x32);
    _w8(_buf, &i, (uint8_t)mem_type);
    _w16(_buf, &i, address);
    _w8(_buf, &i, length);
    _send(_buf, i);

    // Response: AA 32 type addr_hi addr_lo len data... tail
    uint8_t resp[6 + 0xF0 + 4] = {0};
    size_t  n = _uart_read(resp, 6 + length + 4,
                           DWIN_HANDSHAKE_TIMEOUT_MS);
    if (n > 6 && out_buf) {
        size_t data_len = n - 6 - 4;  // strip header (6) and tail (4)
        for (size_t j = 0; j < data_len; j++) {
            out_buf[j] = resp[6 + j];
        }
        return data_len;
    }
    return 0;
}

void dwin_write_picture_memory(uint8_t pic_id) {
    uint8_t pkt[] = {0x33, 0x5A, 0xA5, (uint8_t)(pic_id & 0x0F)};
    _send(pkt, sizeof(pkt));
}

void dwin_update(void) {
    uint8_t pkt[] = {0x3D};
    _send_fast(pkt, sizeof(pkt));
}

void dwin_serial_send(const uint8_t *data, size_t length) {
    size_t i = 0;
    _w8(_buf, &i, 0x39);
    for (size_t j = 0; j < length && i < DWIN_BUF_SIZE; j++) {
        _w8(_buf, &i, data[j]);
    }
    _send_fast(_buf, i);
}

// ===========================================================================
// (3) Drawing commands
// ===========================================================================

void dwin_clear(uint16_t color) {
    size_t i = 0;
    _w8(_buf, &i, 0x01);
    _w16(_buf, &i, color);
    _send_fast(_buf, i);
}

void dwin_draw_points(uint16_t color, uint8_t nx, uint8_t ny,
                      const uint16_t *xs, const uint16_t *ys, size_t count) {
    size_t i = 0;
    _w8(_buf, &i, 0x02);
    _w16(_buf, &i, color);
    _w8(_buf, &i, nx & 0x0F);
    _w8(_buf, &i, ny & 0x0F);
    for (size_t j = 0; j < count && i + 4 < DWIN_BUF_SIZE; j++) {
        _w16(_buf, &i, xs[j]);
        _w16(_buf, &i, ys[j]);
    }
    _send_fast(_buf, i);
}

void dwin_draw_pixel(uint16_t color, uint16_t x, uint16_t y) {
    dwin_draw_points(color, 1, 1, &x, &y, 1);
}

void dwin_draw_line(uint16_t color,
                    uint16_t x1, uint16_t y1,
                    uint16_t x2, uint16_t y2) {
    size_t i = 0;
    _w8(_buf, &i, 0x03);
    _w16(_buf, &i, color);
    _w16(_buf, &i, x1); _w16(_buf, &i, y1);
    _w16(_buf, &i, x2); _w16(_buf, &i, y2);
    _send_fast(_buf, i);
}

void dwin_draw_polyline(uint16_t color,
                        const uint16_t *xs, const uint16_t *ys,
                        uint8_t count) {
    size_t i = 0;
    _w8(_buf, &i, 0x03);
    _w16(_buf, &i, color);
    for (uint8_t j = 0; j < count && i + 4 <= DWIN_BUF_SIZE; j++) {
        _w16(_buf, &i, xs[j]);
        _w16(_buf, &i, ys[j]);
    }
    _send_fast(_buf, i);
}

void dwin_draw_hline(uint16_t color, uint16_t x, uint16_t y, uint16_t length) {
    dwin_draw_line(color, x, y, x + length, y);
}

void dwin_draw_vline(uint16_t color, uint16_t x, uint16_t y, uint16_t length) {
    dwin_draw_line(color, x, y, x, y + length);
}

void dwin_draw_rectangle(dwin_rect_mode_t mode, uint16_t color,
                          uint16_t x1, uint16_t y1,
                          uint16_t x2, uint16_t y2) {
    size_t i = 0;
    _w8(_buf, &i, 0x05);
    _w8(_buf, &i, (uint8_t)(mode & 0x03));
    _w16(_buf, &i, color);
    _w16(_buf, &i, x1); _w16(_buf, &i, y1);
    _w16(_buf, &i, x2); _w16(_buf, &i, y2);
    _send_fast(_buf, i);
}

void dwin_move_area(dwin_move_mode_t mode, dwin_move_dir_t direction,
                    uint16_t distance, uint16_t fill_color,
                    uint16_t x1, uint16_t y1,
                    uint16_t x2, uint16_t y2) {
    size_t i = 0;
    _w8(_buf, &i, 0x09);
    _w8(_buf, &i, (uint8_t)((mode & 0x80) | (direction & 0x0F)));
    _w16(_buf, &i, distance);
    _w16(_buf, &i, fill_color);
    _w16(_buf, &i, x1); _w16(_buf, &i, y1);
    _w16(_buf, &i, x2); _w16(_buf, &i, y2);
    _send_fast(_buf, i);
}

// ===========================================================================
// (4) Text commands
// ===========================================================================

void dwin_draw_string(const char *text,
                      uint16_t x, uint16_t y,
                      dwin_font_t font,
                      uint16_t color, uint16_t bcolor,
                      bool width_adjust, bool show_bg) {
    size_t i = 0;
    uint8_t mode = (uint8_t)(
        (width_adjust ? 0x80 : 0x00) |
        (show_bg      ? 0x40 : 0x00) |
        (font & 0x0F)
    );
    _w8(_buf, &i, 0x11);
    _w8(_buf, &i, mode);
    _w16(_buf, &i, color);
    _w16(_buf, &i, bcolor);
    _w16(_buf, &i, x);
    _w16(_buf, &i, y);
    while (*text && i < DWIN_BUF_SIZE - 1) {
        _w8(_buf, &i, (uint8_t)*text++);
    }
    _send_fast(_buf, i);
}

void dwin_draw_number(int32_t value,
                      uint16_t x, uint16_t y,
                      uint8_t int_digits, uint8_t dec_digits,
                      dwin_font_t font,
                      uint16_t color, uint16_t bcolor,
                      bool is_signed, bool zero_fill,
                      bool show_leading_zero, bool show_bg) {
    size_t i = 0;
    uint8_t mode = (uint8_t)(
        (show_bg           ? 0x80 : 0x00) |
        (is_signed         ? 0x40 : 0x00) |
        (zero_fill         ? 0x20 : 0x00) |
        (show_leading_zero ? 0x10 : 0x00) |
        (font & 0x0F)
    );
    _w8(_buf, &i, 0x14);
    _w8(_buf, &i, mode);
    _w16(_buf, &i, color);
    _w16(_buf, &i, bcolor);
    _w8(_buf, &i, int_digits);
    _w8(_buf, &i, dec_digits);
    _w16(_buf, &i, x);
    _w16(_buf, &i, y);
    // 8-byte big-endian value
    uint32_t uval = (uint32_t)value;
    _w8(_buf, &i, 0); _w8(_buf, &i, 0); _w8(_buf, &i, 0); _w8(_buf, &i, 0);
    _w8(_buf, &i, (uval >> 24) & 0xFF);
    _w8(_buf, &i, (uval >> 16) & 0xFF);
    _w8(_buf, &i, (uval >> 8)  & 0xFF);
    _w8(_buf, &i,  uval        & 0xFF);
    _send_fast(_buf, i);
}

// ===========================================================================
// (5) Picture and icon commands
// ===========================================================================

void dwin_show_qrcode(const char *data,
                      uint16_t x, uint16_t y,
                      uint8_t pixel_size) {
    size_t i = 0;
    _w8(_buf, &i, 0x21);
    _w16(_buf, &i, x);
    _w16(_buf, &i, y);
    _w8(_buf, &i, pixel_size & 0x0F);
    while (*data && i < DWIN_BUF_SIZE - 1) {
        _w8(_buf, &i, (uint8_t)*data++);
    }
    _send(_buf, i);
}

void dwin_show_jpeg(uint8_t pic_id) {
    size_t i = 0;
    _w8(_buf, &i, 0x22);
    _w16(_buf, &i, pic_id & 0x0F);
    _send(_buf, i);
}

void dwin_show_icon(uint8_t lib_id, uint8_t icon_id,
                    uint16_t x, uint16_t y,
                    bool show_bg, bool restore_bg,
                    uint8_t filter_strength) {
    size_t i = 0;
    uint8_t mode = (uint8_t)(
        (show_bg          ? 0x80 : 0x00) |
        (restore_bg       ? 0x40 : 0x00) |
        (filter_strength  ? 0x20 : 0x00) |
        (lib_id & 0x0F)
    );
    _w8(_buf, &i, 0x23);
    _w16(_buf, &i, x);
    _w16(_buf, &i, y);
    _w8(_buf, &i, mode);
    _w8(_buf, &i, icon_id);
    _send(_buf, i);
}

void dwin_show_icon_from_sram(uint16_t address,
                               uint16_t x, uint16_t y,
                               bool show_bg,
                               uint8_t filter_strength) {
    size_t i = 0;
    uint8_t mode = (uint8_t)(
        (show_bg         ? 0x80 : 0x00) |
        (filter_strength ? 0x20 : 0x00)
    );
    _w8(_buf, &i, 0x24);
    _w16(_buf, &i, x);
    _w16(_buf, &i, y);
    _w8(_buf, &i, mode);
    _w16(_buf, &i, address);
    _send(_buf, i);
}

void dwin_decompress_jpeg_to_virtual(uint8_t pic_id) {
    uint8_t pkt[] = {0x25, (uint8_t)(pic_id & 0x0F),
                            (uint8_t)(pic_id & 0x0F)};
    _send(pkt, sizeof(pkt));
}

void dwin_copy_from_virtual_area(uint16_t src_x1, uint16_t src_y1,
                                  uint16_t src_x2, uint16_t src_y2,
                                  uint16_t dst_x,  uint16_t dst_y,
                                  uint8_t virtual_area,
                                  bool show_bg, bool restore_bg,
                                  uint8_t filter_strength) {
    size_t i = 0;
    uint8_t mode = (uint8_t)(
        (show_bg         ? 0x80 : 0x00) |
        (restore_bg      ? 0x40 : 0x00) |
        (filter_strength ? 0x20 : 0x00) |
        (virtual_area & 0x01)
    );
    _w8(_buf, &i, 0x27);
    _w8(_buf, &i, mode);
    _w16(_buf, &i, src_x1); _w16(_buf, &i, src_y1);
    _w16(_buf, &i, src_x2); _w16(_buf, &i, src_y2);
    _w16(_buf, &i, dst_x);  _w16(_buf, &i, dst_y);
    _send(_buf, i);
}

void dwin_set_icon_animation(uint8_t anim_id, bool enabled,
                              uint8_t lib_id,
                              uint8_t icon_start, uint8_t icon_end,
                              uint16_t x, uint16_t y,
                              uint8_t interval_10ms,
                              bool start_from_beginning) {
    size_t i = 0;
    uint8_t mode = (uint8_t)(
        (enabled             ? 0x80 : 0x00) |
        (start_from_beginning? 0x40 : 0x00) |
        (anim_id & 0x0F)
    );
    _w8(_buf, &i, 0x28);
    _w16(_buf, &i, x);
    _w16(_buf, &i, y);
    _w8(_buf, &i, mode);
    _w8(_buf, &i, lib_id & 0x0F);
    _w8(_buf, &i, icon_start);
    _w8(_buf, &i, icon_end);
    _w8(_buf, &i, interval_10ms);
    _send(_buf, i);
}

void dwin_control_animations(uint16_t states) {
    size_t i = 0;
    _w8(_buf, &i, 0x29);
    _w16(_buf, &i, states);
    _send(_buf, i);
}

// ===========================================================================
// (6) Composite helpers
// ===========================================================================

void dwin_draw_progress_bar(uint16_t x, uint16_t y,
                             uint16_t width, uint16_t height,
                             uint8_t percent,
                             uint16_t fg_color, uint16_t bg_color) {
    dwin_draw_rectangle(DWIN_RECT_FILL, bg_color,
                        x, y, x + width, y + height);
    if (percent > 100) percent = 100;
    uint16_t fill = (uint16_t)((uint32_t)width * percent / 100);
    if (fill > 0) {
        dwin_draw_rectangle(DWIN_RECT_FILL, fg_color,
                            x, y, x + fill, y + height);
    }
}

void dwin_fill_screen(uint16_t color) {
    dwin_clear(color);
    dwin_update();
}
