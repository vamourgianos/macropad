// #pragma once

// ---------------------------------------------------------------------------
// Pins
// ---------------------------------------------------------------------------
#define STATUS_LED         C13
#define DWIN_UART_DRIVER   SD1
#define BACKLIGHT_PWM_DRIVER PWMD1
#define BACKLIGHT_PWM_CHANNEL 1

// ---------------------------------------------------------------------------
// Configurable Sizes
// ---------------------------------------------------------------------------
#define MACROPAD_NUM_LAYERS    4
#define MACROPAD_NAME_LEN     16
#define MACROPAD_NUM_ROWS      4
#define MACROPAD_NUM_COLUMNS   5
#define MACROPAD_LABEL_LEN     8
#define MACRO_STEPS            6

// ---------------------------------------------------------------------------
// NON-Configurable Sizes - DO NOT CHANGE!!!
// ---------------------------------------------------------------------------
#define MACROPAD_NUM_KEYS    (MACROPAD_NUM_ROWS * MACROPAD_NUM_COLUMNS)
#define MACRO_STEP_SIZE      2
#define MACRO_SIZE           (1 + (MACRO_STEP_SIZE * MACRO_STEPS))
#define KEY_CONFIG_SIZE      (MACROPAD_LABEL_LEN + MACRO_SIZE)
#define LAYER_CONFIG_SIZE    (MACROPAD_NAME_LEN + (MACROPAD_NUM_KEYS * KEY_CONFIG_SIZE))
#define ACTIVE_CONFIG_SIZE   4
#define ENCODER_CONFIG_SIZE  (2*KEY_CONFIG_SIZE)
#define MACROPAD_CONFIG_SIZE (ACTIVE_CONFIG_SIZE + (MACROPAD_NUM_LAYERS * LAYER_CONFIG_SIZE) + ENCODER_CONFIG_SIZE)

#define VIA_EEPROM_CUSTOM_CONFIG_SIZE MACROPAD_CONFIG_SIZE

