#!/usr/bin/env python3
"""
configure.py — MacroPad Display Configurator
=============================================
Configures key labels and macros stored in the macropad's EEPROM
via USB Raw HID. No firmware reflash needed.

Requirements:
    pip install hidapi

Usage:
    python3 configure.py                        # GUI mode (default)
    python3 configure.py --cli                  # interactive CLI
    python3 configure.py --layer 0 --name "AUTOCAD"
    python3 configure.py --layer 0 --key 0 --label "Line" --macro "L\\n"
    python3 configure.py --dump
    python3 configure.py --reset

Macro syntax:
    Plain text:  "hello"    → types h,e,l,l,o
    Enter:       "\\n"      → KC_ENT
    Tab:         "\\t"      → KC_TAB
    Escape:      "\\e"      → KC_ESC
    Backspace:   "\\b"      → KC_BSPC
    Ctrl:        "\\cz"     → Ctrl+Z
    Shift:       "\\sA"     → Shift+A  (= capital A)
    Alt:         "\\aa"     → Alt+A
    GUI:         "\\ga"     → GUI+A
    Combined:    "\\c\\sz"  → Ctrl+Shift+Z
"""

import sys
import time
import argparse

# ---------------------------------------------------------------------------
# Constants — must match display_config.h
# ---------------------------------------------------------------------------

VENDOR_ID   = 0xFEED
PRODUCT_ID  = 0xB00B

PACKET_SIZE      = 30
MACROPAD_CHANNEL = 0  # must match keymap_integration.c

CMD_GET_SIZES         = 0x3f
CMD_GET_LAYER_NAME    = 0x40
CMD_SET_LAYER_NAME    = 0x41
CMD_GET_KEY_LABEL     = 0x42
CMD_SET_KEY_LABEL     = 0x43
CMD_GET_KEY_MACRO     = 0x44
CMD_SET_KEY_MACRO     = 0x45
CMD_GET_ENC_KEY_LABEL = 0x46
CMD_SET_ENC_KEY_LABEL = 0x47
CMD_GET_ENC_KEY_MACRO = 0x48
CMD_SET_ENC_KEY_MACRO = 0x49
CMD_RESET_CONFIG      = 0x4a
CMD_GET_ACTIVE        = 0x4b
CMD_SET_ACTIVE        = 0x4c
CMD_REDRAW_DISPLAY    = 0x4d

MOD_LCTL = (1 << 0)
MOD_LSFT = (1 << 1)
MOD_LALT = (1 << 2)
MOD_LGUI = (1 << 3)

_KEYCODE = {
    'a': 4,  'b': 5,  'c': 6,  'd': 7,  'e': 8,  'f': 9,
    'g': 10, 'h': 11, 'i': 12, 'j': 13, 'k': 14, 'l': 15,
    'm': 16, 'n': 17, 'o': 18, 'p': 19, 'q': 20, 'r': 21,
    's': 22, 't': 23, 'u': 24, 'v': 25, 'w': 26, 'x': 27,
    'y': 28, 'z': 29,
    '1': 30, '2': 31, '3': 32, '4': 33, '5': 34,
    '6': 35, '7': 36, '8': 37, '9': 38, '0': 39,
    '\n': 40, '\x1b': 41, '\x08': 42, '\t': 43, ' ': 44,
    '-': 45, '=': 46, '[': 47, ']': 48, '\\': 49,
    ';': 51, "'": 52, '`': 53, ',': 54, '.': 55, '/': 56,
    '!': (30, MOD_LSFT), '@': (31, MOD_LSFT), '#': (32, MOD_LSFT),
    '$': (33, MOD_LSFT), '%': (34, MOD_LSFT), '^': (35, MOD_LSFT),
    '&': (36, MOD_LSFT), '*': (37, MOD_LSFT), '(': (38, MOD_LSFT),
    ')': (39, MOD_LSFT), '_': (45, MOD_LSFT), '+': (46, MOD_LSFT),
    '{': (47, MOD_LSFT), '}': (48, MOD_LSFT), '|': (49, MOD_LSFT),
    ':': (51, MOD_LSFT), '"': (52, MOD_LSFT), '~': (53, MOD_LSFT),
    '<': (54, MOD_LSFT), '>': (55, MOD_LSFT), '?': (56, MOD_LSFT),
}

# ---------------------------------------------------------------------------
# Macro encode / decode
# ---------------------------------------------------------------------------

def parse_macro(macro_str: str, macro_steps: int) -> list:
    steps = []
    s = macro_str.replace('\\n', '\n').replace('\\t', '\t') \
                 .replace('\\e', '\x1b').replace('\\b', '\x08')
    i = 0
    while i < len(s):
        mods = 0
        while i < len(s) and s[i] == '\\':
            if i + 1 >= len(s):
                raise ValueError("Trailing backslash in macro")
            mc = s[i + 1]
            if   mc == 'c': mods |= MOD_LCTL
            elif mc == 's': mods |= MOD_LSFT
            elif mc == 'a': mods |= MOD_LALT
            elif mc == 'g': mods |= MOD_LGUI
            else: raise ValueError(f"Unknown escape \\{mc}")
            i += 2
        if i >= len(s):
            break
        ch = s[i]; i += 1
        entry = _KEYCODE.get(ch.lower())
        if entry is None:
            raise ValueError(f"Unknown character '{ch}'")
        if isinstance(entry, tuple):
            kc, extra = entry
            mods |= extra
        else:
            kc = entry
            if ch.isupper():
                mods |= MOD_LSFT
        steps.append((mods, kc))
    if len(steps) > macro_steps:
        raise ValueError(f"Macro too long ({len(steps)} steps, max {macro_steps})")
    return steps

def encode_macro(steps: list, macro_steps: int) -> bytes:
    buf = bytearray(1+(macro_steps*2))
    buf[0] = len(steps)
    for i, (mods, kc) in enumerate(steps[:macro_steps]):
        buf[1 + i * 2]     = mods
        buf[1 + i * 2 + 1] = kc
    return bytes(buf)

def decode_macro(data: bytes) -> list:
    count = data[0]
    return [(data[1+i*2], data[1+i*2+1]) for i in range(count)]

def macro_to_str(steps: list) -> str:
    # Base reverse map: keycode -> character (letters, digits, punctuation)
    _BASE = {}
    # Shifted special chars: (keycode, MOD_LSFT) -> character (!, @, #, ...)
    _SHIFTED = {}

    for ch, v in _KEYCODE.items():
        if isinstance(v, tuple):
            kc, extra_mod = v
            _SHIFTED[(kc, extra_mod)] = ch
        else:
            _BASE[v] = ch

    # Override control keycodes with their escape-sequence representations
    _BASE.update({40: '\\n', 41: '\\e', 42: '\\b', 43: '\\t'})

    _MODS = [
        (MOD_LCTL, '\\c'),
        (MOD_LSFT, '\\s'),
        (MOD_LALT, '\\a'),
        (MOD_LGUI, '\\g'),
    ]

    out = ''
    for mods, kc in steps:
        # Case 1: shifted special character (!, @, #, …)
        # MOD_LSFT is "consumed" by the character itself
        if (kc, MOD_LSFT) in _SHIFTED and (mods & MOD_LSFT):
            remaining = mods & ~MOD_LSFT
            for bit, sym in _MODS:
                if remaining & bit:
                    out += sym
            out += _SHIFTED[(kc, MOD_LSFT)]

        # Case 2: uppercase letter (MOD_LSFT + letter keycode)
        elif (mods & MOD_LSFT) and kc in _BASE and _BASE[kc].isalpha():
            remaining = mods & ~MOD_LSFT
            for bit, sym in _MODS:
                if remaining & bit:
                    out += sym
            out += _BASE[kc].upper()

        # Case 3: plain keycode, emit all active mod prefixes
        elif kc in _BASE:
            for bit, sym in _MODS:
                if mods & bit:
                    out += sym
            out += _BASE[kc]

        # Case 4: unknown keycode
        else:
            for bit, sym in _MODS:
                if mods & bit:
                    out += sym
            out += f'[{kc}]'
    return out

# ---------------------------------------------------------------------------
# HID device
# ---------------------------------------------------------------------------

class MacroPad:
    def __init__(self, vendor_id=VENDOR_ID, product_id=PRODUCT_ID):
        try:
            import hid
        except ImportError:
            raise ImportError(
                "HID library not found. Install it with:\n"
                "  pip install hid")
        # On Linux, enumerate returns multiple interfaces.
        # Raw HID is always the last interface (interface 1.2 with debug
        # disabled, 1.3 with debug enabled). Open by path to be explicit.
        devices = hid.enumerate(vendor_id, product_id)
        if not devices:
            raise OSError(f"Device {vendor_id:04X}:{product_id:04X} not found. "
                          f"Is the keyboard connected?")
        # Sort by path and take the last interface — that's raw HID
        devices.sort(key=lambda d: d['path'])
        raw_hid_path = devices[1]['path']

        self._dev = hid.device()
        self._dev.open_path(raw_hid_path)
        self._dev.set_nonblocking(False)
        self._get_sizes()

    def close(self):
        self._dev.close()

    def __enter__(self): return self
    def __exit__(self, *a): self.close()

    def _send(self, data: bytes, is_get: bool = False) -> bytes:
        # Packet format:
        #   [0]   = HID report ID (0x00)
        #   [1]   = command_id (0x09) - VIA custom command
        #   [2]   = MACROPAD_CHANNEL (0x00) — Custom channel
        #   [3+]  = sub-command + payload
        pkt = bytearray(PACKET_SIZE + 3)
        pkt[0] = 0x00              # HID report ID
        pkt[1] = 0x09              # command_id
        pkt[2] = MACROPAD_CHANNEL  # our channel
        for i, b in enumerate(data[:PACKET_SIZE]):
            pkt[i+3] = b
        self._dev.write(bytes(pkt))
        time.sleep(0.05)
        resp = bytes(self._dev.read(PACKET_SIZE+2, timeout_ms=1000))
        return resp[2:]

    def _get_sizes(self):
        resp = self._send(bytes([CMD_GET_SIZES]))
        print(f'  [DEBUG] get_sizes() RECEIVED: len={len(resp)} bytes: {resp.hex()}')
        if resp[0] != CMD_GET_SIZES:
            raise ValueError(f"Unexpected response to get_sizes: {resp.hex()}")
        self._NUM_LAYERS  = resp[1]
        self._NAME_LEN    = resp[2]
        self._NUM_ROWS    = resp[3]
        self._NUM_COLUMNS = resp[4]
        self._LABEL_LEN   = resp[5]
        self._MACRO_STEPS = resp[6]
        self._NUM_KEYS    = self._NUM_ROWS * self._NUM_COLUMNS
        print(f'  [DEBUG] Active sizes: layers={self._NUM_LAYERS} name_len={self._NAME_LEN} rows={self._NUM_ROWS} columns={self._NUM_COLUMNS} label_len={self._LABEL_LEN} macro_steps={self._MACRO_STEPS}')

    def get_layer_name(self, layer: int) -> str:
        header = bytes([CMD_GET_LAYER_NAME, layer])
        resp = self._send(header)
        print(f'  [DEBUG] get_layer_name({layer}) RECEIVED: len={len(resp)} bytes: {resp.hex()}')
        if resp[0:len(header)] != header:
            raise ValueError(f"Unexpected response to get_layer_name: {resp.hex()}")
        return resp[len(header):len(header)+self._NAME_LEN+1].split(b'\x00')[0].decode('ascii','replace')

    def set_layer_name(self, layer: int, name: str):
        packet = bytearray(PACKET_SIZE)
        header = bytes([CMD_SET_LAYER_NAME, layer])
        nb = name.encode('ascii')[:self._NAME_LEN]
        assert len(header) + len(nb) <= PACKET_SIZE
        packet[0:len(header)] = header
        packet[len(header):len(header)+len(nb)] = nb
        print(f'  [DEBUG] set_layer_name({layer}) SENT: len={len(packet)} bytes: {packet.hex()}')
        self._send(bytes(packet))

    def get_key_label(self, layer: int, key: int) -> str:
        header = bytes([CMD_GET_KEY_LABEL, layer, key])
        resp = self._send(header)
        print(f'  [DEBUG] get_key_label({layer},{key}) RECEIVED: len={len(resp)} bytes: {resp.hex()}')
        if resp[0:len(header)] != header:
            raise ValueError(f"Unexpected response to get_key_label: {resp.hex()}")
        if len(resp) < (len(header) + self._LABEL_LEN + 1):
            return ''
        label = bytearray(resp[len(header):len(header)+self._LABEL_LEN + 1])
        label[self._LABEL_LEN] = 0
        return label.split(b'\x00')[0].decode('ascii','replace')

    def set_key_label(self, layer: int, key: int, label: str):
        packet = bytearray(PACKET_SIZE)
        header = bytes([CMD_SET_KEY_LABEL, layer, key])
        nb = label.encode('ascii')[:self._LABEL_LEN]
        assert len(header) + len(nb) <= PACKET_SIZE
        packet[0:len(header)] = header
        packet[len(header):len(header)+len(nb)] = nb
        print(f'  [DEBUG] set_key_label({layer},{key}) SENT: len={len(packet)} bytes: {packet.hex()}')
        self._send(bytes(packet))

    def get_key_macro(self, layer: int, key: int) -> str:
        header = bytes([CMD_GET_KEY_MACRO, layer, key])
        resp = self._send(header)
        print(f'  [DEBUG] get_key_macro({layer},{key}) RECEIVED: len={len(resp)} bytes: {resp.hex()}')
        if resp[0:len(header)] != header:
            raise ValueError(f"Unexpected response to get_key_macro: {resp.hex()}")
        macro_size = (self._MACRO_STEPS*2) + 1
        if len(resp) < (len(header) + macro_size):
            return ''
        steps = decode_macro(resp[len(header):(len(header) + macro_size)])
        return macro_to_str(steps)

    def set_key_macro(self, layer: int, key: int, macro: str):
        steps = parse_macro(macro, self._MACRO_STEPS)
        packet = bytearray(PACKET_SIZE)
        header = bytes([CMD_SET_KEY_MACRO, layer, key])
        assert len(header) + len(macro) <= PACKET_SIZE
        mb = encode_macro(steps, self._MACRO_STEPS)
        packet[0:len(header)] = header
        packet[len(header):len(header)+len(mb)] = mb
        print(f'  [DEBUG] set_key_macro({layer},{key}) SENT: len={len(packet)} bytes: {packet.hex()}')
        self._send(bytes(packet))
    
    def get_key(self, layer: int, key: int) -> dict:
        return {'label': f'{self.get_key_label(layer, key)}',
                'macro_str': f'{self.get_key_macro(layer, key)}'}

    def set_key(self, layer: int, key: int, label: str, macro: str):
        self.set_key_label(layer, key, label)
        self.set_key_macro(layer, key, macro)

    def get_encoder_key_label(self, clockwise: int) -> str:
        header = bytes([CMD_GET_ENC_KEY_LABEL, clockwise])
        resp = self._send(header)
        print(f'  [DEBUG] get_encoder_key_label({clockwise}) RECEIVED: len={len(resp)} bytes: {resp.hex()}')
        if resp[0:len(header)] != header:
            raise ValueError(f"Unexpected response to get_encoder_key_label: {resp.hex()}")
        if len(resp) < (len(header) + self._LABEL_LEN + 1):
            return ''
        label = bytearray(resp[len(header):len(header)+self._LABEL_LEN + 1])
        label[self._LABEL_LEN] = 0
        return label.split(b'\x00')[0].decode('ascii','replace')

    def set_encoder_key_label(self, clockwise: int, label: str):
        packet = bytearray(PACKET_SIZE)
        header = bytes([CMD_SET_ENC_KEY_LABEL, clockwise])
        nb = label.encode('ascii')[:self._LABEL_LEN]
        assert len(header) + len(nb) <= PACKET_SIZE
        packet[0:len(header)] = header
        packet[len(header):len(header)+len(nb)] = nb
        print(f'  [DEBUG] set_encoder_key_label({clockwise}) SENT: len={len(packet)} bytes: {packet.hex()}')
        self._send(bytes(packet))

    def get_encoder_key_macro(self, clockwise: int) -> str:
        header = bytes([CMD_GET_ENC_KEY_MACRO, clockwise])
        resp = self._send(header)
        print(f'  [DEBUG] get_encoder_key_macro({clockwise}) RECEIVED: len={len(resp)} bytes: {resp.hex()}')
        if resp[0:len(header)] != header:
            raise ValueError(f"Unexpected response to get_encoder_key_macro: {resp.hex()}")
        macro_size = (self._MACRO_STEPS*2) + 1
        if len(resp) < (len(header) + macro_size):
            return ''
        steps = decode_macro(resp[len(header):(len(header) + macro_size)])
        return macro_to_str(steps)

    def set_encoder_key_macro(self, clockwise: int, macro: str):
        steps = parse_macro(macro, self._MACRO_STEPS)
        packet = bytearray(PACKET_SIZE)
        header = bytes([CMD_SET_ENC_KEY_MACRO, clockwise])
        assert len(header) + len(macro) <= PACKET_SIZE
        mb = encode_macro(steps, self._MACRO_STEPS)
        packet[0:len(header)] = header
        packet[len(header):len(header)+len(mb)] = mb
        print(f'  [DEBUG] set_encoder_key_macro({clockwise}) SENT: len={len(packet)} bytes: {packet.hex()}')
        self._send(bytes(packet))
    
    def get_encoder_key(self, clockwise: int) -> dict:
        return {'label': f'{self.get_encoder_key_label(clockwise)}',
                'macro_str': f'{self.get_encoder_key_macro(clockwise)}'}

    def set_encoder_key(self, clockwise: int, label: str, macro: str):
        self.set_encoder_key_label(clockwise, label)
        self.set_encoder_key_macro(clockwise, macro)

    def reset(self):
        self._send(bytes([CMD_RESET_CONFIG]))

    def redraw_display(self):
        self._send(bytes([CMD_REDRAW_DISPLAY]))

    def load_all(self) -> dict:
        """Load all layers. Returns list of dicts."""
        enc_layer = {'name': 'Encoder', 'keys': [
            self.get_encoder_key(0),
            self.get_encoder_key(1)
        ]}
        layers = []
        for l in range(self._NUM_LAYERS):
            layer = {'name': self.get_layer_name(l), 'keys': []}
            for k in range(self._NUM_KEYS):
                layer['keys'].append(self.get_key(l, k))
            layers.append(layer)
        return {'enc_layer': enc_layer, 'layers': layers}

    def save_all(self, layers: dict):
        """Save all layers from list of dicts."""
        for k, key in enumerate(layers['enc_layer']['keys']):
            self.set_encoder_key(k, key['label'], key['macro_str'])
        for l, layer in enumerate(layers['layers']):
            self.set_layer_name(l, layer['name'])
            for k, key in enumerate(layer['keys']):
                self.set_key(l, k, key['label'], key['macro_str'])

# ---------------------------------------------------------------------------
# GUI
# ---------------------------------------------------------------------------

def run_gui():
    import tkinter as tk
    from tkinter import ttk, messagebox

    class App(tk.Tk):
        def __init__(self):
            super().__init__()
            self.title("MacroPad Configurator")
            self.resizable(False, False)
            self._pad = None
            self._build_ui()

        # -------------------------------------------------------------------
        # UI construction
        # -------------------------------------------------------------------

        def _build_ui(self):
            # Top toolbar
            toolbar = tk.Frame(self, pady=4, padx=8)
            toolbar.pack(side=tk.TOP, fill=tk.X)

            tk.Label(toolbar, text="VID:").pack(side=tk.LEFT)
            self._vid = tk.Entry(toolbar, width=8)
            self._vid.insert(0, f"0x{VENDOR_ID:04X}")
            self._vid.pack(side=tk.LEFT, padx=(0, 8))

            tk.Label(toolbar, text="PID:").pack(side=tk.LEFT)
            self._pid = tk.Entry(toolbar, width=8)
            self._pid.insert(0, f"0x{PRODUCT_ID:04X}")
            self._pid.pack(side=tk.LEFT, padx=(0, 16))

            tk.Button(toolbar, text="⇄  Connect to keyboard",
                      command=self._connect, width=22).pack(side=tk.LEFT, padx=4)
            tk.Button(toolbar, text="⬇  Load from keyboard",
                      command=self._load, width=22).pack(side=tk.LEFT, padx=4)
            tk.Button(toolbar, text="⬆  Save to keyboard",
                      command=self._save, width=22).pack(side=tk.LEFT, padx=4)
            tk.Button(toolbar, text="↺  Reset keyboard",
                      command=self._reset, width=18,
                      fg='red').pack(side=tk.LEFT, padx=4)

            # Status bar
            self._status = tk.StringVar(value="Not connected.")
            tk.Label(self, textvariable=self._status,
                     anchor=tk.W, relief=tk.SUNKEN,
                     padx=6).pack(side=tk.BOTTOM, fill=tk.X)

            # create the frames
            cfg_frame = ttk.LabelFrame(self, text="Keyboard Config", padding=6)
            cfg_frame.pack(fill=tk.X, padx=8, pady=(4, 0))
            # Read-only config fields — updated after each successful connection
            cfg_fields = [
                ("Layers",      "_cfg_layers"),
                ("Rows",        "_cfg_rows"),
                ("Columns",     "_cfg_columns"),
                ("Keys",        "_cfg_keys"),
                ("Label len",   "_cfg_label_len"),
                ("Macro steps", "_cfg_macro_steps"),
            ]
            for col, (lbl, attr) in enumerate(cfg_fields):
                tk.Label(cfg_frame, text=lbl + ":",
                         font=("TkDefaultFont", 8, "bold")).grid(
                    row=0, column=col*2, padx=(10 if col else 4, 2), sticky=tk.E)
                var = tk.StringVar(value="—")
                setattr(self, attr, var)
                tk.Label(cfg_frame, textvariable=var,
                         font=("Courier", 9), width=5, anchor=tk.W).grid(
                    row=0, column=col*2+1, padx=(0, 6), sticky=tk.W)

            enc_frame = ttk.LabelFrame(self, text="Encoder", padding=8)
            enc_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)
            layers_frame = ttk.LabelFrame(self, text="Layers", padding=8)
            layers_frame.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)
            help_frame = tk.LabelFrame(self, text="Macro Syntax", padx=8, pady=4)
            help_frame.pack(fill=tk.X, padx=8, pady=(0, 4))

            # start filling them with empty content
            self._grid_frame = tk.Frame(enc_frame)
            self._grid_frame.pack(fill=tk.BOTH)
            self._enc_widgets  = []

            # Tabs — one per layer
            self._nb = ttk.Notebook(layers_frame, padding=8)
            self._nb.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)
            self._layer_frames = []
            self._layer_names  = []   # StringVar per layer
            self._key_widgets  = []   # list of lists: [layer][key] = (label_var, macro_var)

            # Macro help
            help_text = (
                "\\n=Enter  \\t=Tab  \\e=Esc  \\b=Bksp  "
                "\\c=Ctrl  \\s=Shift  \\a=Alt  \\g=GUI  "
                "  |  Examples:  L\\n → L+Enter    \\cz → Ctrl+Z    \\c\\sz → Ctrl+Shift+Z"
            )
            tk.Label(help_frame, text=help_text,
                     font=("Courier", 9)).pack(anchor=tk.W)

        def _build_encoder_frame(self):
            # Clear any widgets from a previous connect
            for w in self._grid_frame.winfo_children():
                w.destroy()
            self._enc_widgets = []

            # Directions: K00 = CCW, K01 = CW
            DIRECTIONS = ["CCW", "CW"]

            # Centered row: ↺  [K00 frame]  [K01 frame]  ↻
            center = tk.Frame(self._grid_frame)
            center.pack(expand=True)

            tk.Label(center, text="↺", font=("TkDefaultFont", 22)
                     ).grid(row=0, column=0, padx=12)

            for key_idx in range(2):
                direction = DIRECTIONS[key_idx]
                box = ttk.LabelFrame(center,
                                     text=f" K{key_idx:02d} — {direction} ",
                                     padding=6)
                box.grid(row=0, column=key_idx + 1, padx=10, pady=6)

                label_var = tk.StringVar()
                tk.Entry(box, textvariable=label_var,
                         width=16, font=("Courier", 10)).pack(fill=tk.X)

                tk.Label(box, text="Macro", font=("TkDefaultFont", 8)).pack(anchor=tk.W, pady=(4, 0))
                macro_var = tk.StringVar()
                macro_entry = tk.Entry(box, textvariable=macro_var,
                                       width=20, font=("Courier", 10))
                macro_entry.pack(fill=tk.X)
                macro_entry.bind('<FocusOut>',
                    lambda e, mv=macro_var, ki=key_idx:
                        self._validate_macro(mv, e.widget, ki))

                self._enc_widgets.append((label_var, macro_var))

            tk.Label(center, text="↻", font=("TkDefaultFont", 22)
                     ).grid(row=0, column=3, padx=12)

        def _build_layer_tab(self, layer_idx: int):
            outer = tk.Frame(self._nb)

            # Layer name row
            name_row = tk.Frame(outer)
            name_row.pack(fill=tk.X, pady=(4, 8))
            tk.Label(name_row, text="Layer Name:", width=12,
                     anchor=tk.E).pack(side=tk.LEFT)
            name_var = tk.StringVar(value=f"LAYER {layer_idx}")
            tk.Entry(name_row, textvariable=name_var,
                     width=20, font=("TkDefaultFont", 10, "bold")
                     ).pack(side=tk.LEFT, padx=4)

            # Key grid — one LabelFrame per key, placed at its (row, col) position
            grid_frame = tk.Frame(outer)
            grid_frame.pack(fill=tk.BOTH, padx=8, pady=4)

            key_vars = []
            for key_idx in range(self._pad._NUM_KEYS):
                grid_row, grid_col = self._key_positions[key_idx]

                box = ttk.LabelFrame(grid_frame,
                                     text=f" K{key_idx:02d} ({grid_row},{grid_col}) ",
                                     padding=4)
                box.grid(row=grid_row, column=grid_col,
                         padx=4, pady=4, sticky=tk.NSEW)

                label_var = tk.StringVar()
                tk.Entry(box, textvariable=label_var,
                         width=12, font=("Courier", 10)).pack(fill=tk.X)

                tk.Label(box, text="Macro", font=("TkDefaultFont", 8)).pack(anchor=tk.W, pady=(4, 0))
                macro_var = tk.StringVar()
                macro_entry = tk.Entry(box, textvariable=macro_var,
                                       width=16, font=("Courier", 10))
                macro_entry.pack(fill=tk.X)
                macro_entry.bind('<FocusOut>',
                    lambda e, mv=macro_var, ki=key_idx:
                        self._validate_macro(mv, e.widget, ki))

                key_vars.append((label_var, macro_var))

            return outer, name_var, key_vars

        # -------------------------------------------------------------------
        # Macro validation (live)
        # -------------------------------------------------------------------

        def _validate_macro(self, macro_var, widget, key_idx):
            val = macro_var.get()
            if not val:
                widget.config(bg='white')
                return
            try:
                parse_macro(val, self._pad._MACRO_STEPS)
                widget.config(bg='#e8ffe8')  # green tint = ok
            except ValueError as e:
                widget.config(bg='#ffe8e8')  # red tint = error

        # -------------------------------------------------------------------
        # Device connection
        # -------------------------------------------------------------------

        def _connect(self, load = True):
            if self._pad:
                self._pad.close()
            # Clear existing tabs
            for tab in self._nb.tabs():
                self._nb.forget(tab)
            self._nb.children.clear()
            self._layer_frames.clear()
            self._layer_names.clear()
            self._key_widgets.clear()
            try:
                vid = int(self._vid.get(), 0)
                pid = int(self._pid.get(), 0)
                self._pad = MacroPad(vid, pid)
            except Exception as e:
                messagebox.showerror("Connection Error",
                    f"Could not connect to MacroPad:\n{e}\n\n"
                    f"Check VID/PID and that the keyboard is connected.")
                return

            # Derive key positions from the actual keyboard geometry
            self._key_positions = [
                (r, c)
                for r in range(self._pad._NUM_ROWS)
                for c in range(self._pad._NUM_COLUMNS)
            ]

            # Update the Keyboard Config display
            self._cfg_layers.set(str(self._pad._NUM_LAYERS))
            self._cfg_rows.set(str(self._pad._NUM_ROWS))
            self._cfg_columns.set(str(self._pad._NUM_COLUMNS))
            self._cfg_keys.set(str(self._pad._NUM_KEYS))
            self._cfg_label_len.set(str(self._pad._LABEL_LEN))
            self._cfg_macro_steps.set(str(self._pad._MACRO_STEPS))
            self._build_encoder_frame()
            for l in range(self._pad._NUM_LAYERS):
                frame, name_var, key_vars = self._build_layer_tab(l)
                self._nb.add(frame, text=f"Layer {l}")
                self._layer_frames.append(frame)
                self._layer_names.append(name_var)
                self._key_widgets.append(key_vars)
            
            if load:
                self._load()

        # -------------------------------------------------------------------
        # Load from keyboard
        # -------------------------------------------------------------------
        def _load(self):
            if not self._pad:
                self._pad = self._connect(False)
            try:
                self._status.set("Loading...")
                self.update()
                layers = self._pad.load_all()
                for k, key in enumerate(layers['enc_layer']['keys']):
                    self._enc_widgets[k][0].set(key['label'])
                    self._enc_widgets[k][1].set(key['macro_str'])
                for l, layer in enumerate(layers['layers']):
                    self._layer_names[l].set(layer['name'])
                    for k, key in enumerate(layer['keys']):
                        self._key_widgets[l][k][0].set(key['label'])
                        self._key_widgets[l][k][1].set(key['macro_str'])
                self._status.set(
                    f"Loaded {self._pad._NUM_LAYERS} layers × {self._pad._NUM_KEYS} keys from keyboard.")
            except Exception as e:
                messagebox.showerror("Load Error", str(e))
                self._status.set("Load failed.")

        # -------------------------------------------------------------------
        # Save to keyboard
        # -------------------------------------------------------------------

        def _save(self):
            # Validate all macros first
            if not self._pad:
                self._pad = self._connect(False)
            errors = []
            for clockwise in range(2):
                macro = self._enc_widgets[clockwise][1].get()
                if macro:
                    try:
                        parse_macro(macro, self._pad._MACRO_STEPS)
                    except ValueError as e:
                        errors.append(f"Encoder Key {clockwise}: {e}")
            for l in range(self._pad._NUM_LAYERS):
                for k in range(self._pad._NUM_KEYS):
                    macro = self._key_widgets[l][k][1].get()
                    if macro:
                        try:
                            parse_macro(macro, self._pad._MACRO_STEPS)
                        except ValueError as e:
                            errors.append(f"Layer {l} Key {k}: {e}")
            if errors:
                messagebox.showerror("Validation Error",
                    "Fix these macro errors before saving:\n\n" +
                    "\n".join(errors))
                return
            try:
                self._status.set("Saving...")
                self.update()
                enc_layer = {'name': 'Encoder', 'keys': [
                    {'label': self._enc_widgets[k][0].get(),
                     'macro_str': self._enc_widgets[k][1].get()}
                    for k in range(2)
                ]}
                key_layers = []
                for l in range(self._pad._NUM_LAYERS):
                    layer = {
                        'name': self._layer_names[l].get(),
                        'keys': []
                    }
                    for k in range(self._pad._NUM_KEYS):
                        label = self._key_widgets[l][k][0].get()
                        macro = self._key_widgets[l][k][1].get()
                        layer['keys'].append({'label': label,
                                              'macro_str': macro})
                    key_layers.append(layer)
                layers = {'enc_layer': enc_layer, 'layers': key_layers}
                self._pad.save_all(layers)
                self._pad.redraw_display()
                self._status.set(
                    f"Saved {self._pad._NUM_LAYERS} layers × {self._pad._NUM_KEYS} keys to keyboard.")
                messagebox.showinfo("Saved",
                    "Configuration saved to keyboard successfully.")
            except Exception as e:
                messagebox.showerror("Save Error", str(e))
                self._status.set("Save failed.")

        # -------------------------------------------------------------------
        # Reset keyboard
        # -------------------------------------------------------------------

        def _reset(self):
            if not messagebox.askyesno("Reset",
                "Reset ALL keyboard configuration to defaults?\n"
                "This cannot be undone."):
                return
            if not self._pad:
                self._pad = self._connect(False)
            try:
                self._pad.reset()
                self._status.set("Keyboard reset to defaults.")
                messagebox.showinfo("Reset", "Keyboard reset to defaults.")
            except Exception as e:
                messagebox.showerror("Reset Error", str(e))

    app = App()
    app.mainloop()

# ---------------------------------------------------------------------------
# Interactive CLI
# ---------------------------------------------------------------------------

def run_cli():
    print("\nMacroPad Configurator — Interactive CLI")
    print("Commands: name, key, dump, reset, quit\n")

    vid = VENDOR_ID
    pid = PRODUCT_ID

    try:
        pad = MacroPad(vid, pid)
        print(f"Connected.")
    except Exception as e:
        print(f"Error: {e}")
        return

    with pad:
        while True:
            try:
                cmd = input(">> ").strip().lower()
            except (EOFError, KeyboardInterrupt):
                break

            if cmd in ('q', 'quit'):
                break
            elif cmd == 'dump':
                for l in range(pad._NUM_LAYERS):
                    name = pad.get_layer_name(l)
                    print(f"\nLayer {l}: {name}")
                    for k in range(pad._NUM_KEYS):
                        cfg = pad.get_key(l, k)
                        r, c = k // 4, k % 4
                        print(f"  K{k:02d}({r},{c}) '{cfg['label']:<8}' '{cfg['macro_str']}'")
            elif cmd == 'name':
                layer = int(input(f"  Layer (0-{pad._NUM_LAYERS-1}): "))
                name  = input("  Name: ").strip()
                pad.set_layer_name(layer, name)
                print("  Saved.")
            elif cmd == 'key':
                layer = int(input(f"  Layer (0-{pad._NUM_LAYERS-1}): "))
                key   = int(input(f"  Key   (0-{pad._NUM_KEYS-1}):   "))
                cfg   = pad.get_key(layer, key)
                print(f"  Current: label='{cfg['label']}' macro='{cfg['macro_str']}'")
                label = input("  Label: ").strip()
                macro = input("  Macro: ").strip()
                pad.set_key(layer, key, label, macro)
                print("  Saved.")
            elif cmd == 'reset':
                if input("Reset all? (yes/no): ").lower() == 'yes':
                    pad.reset()
                    print("  Reset.")
            elif cmd == 'help':
                print("  name / key / dump / reset / quit")
            else:
                print("  Unknown command.")

# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description='MacroPad Configurator')
    parser.add_argument('--cli',    action='store_true', help='Interactive CLI mode')
    parser.add_argument('--vid',    type=lambda x: int(x, 0), default=VENDOR_ID)
    parser.add_argument('--pid',    type=lambda x: int(x, 0), default=PRODUCT_ID)
    parser.add_argument('--layer',  type=int)
    parser.add_argument('--name',   type=str)
    parser.add_argument('--key',    type=int)
    parser.add_argument('--label',  type=str)
    parser.add_argument('--macro',  type=str)
    parser.add_argument('--dump',   action='store_true')
    parser.add_argument('--reset',  action='store_true')
    args = parser.parse_args()

    # No arguments → GUI
    if len(sys.argv) == 1:
        run_gui()
        return

    # --cli → interactive
    if args.cli:
        run_cli()
        return

    # Command line mode
    try:
        with MacroPad(args.vid, args.pid) as pad:
            if args.reset:
                pad.reset()
                print("Reset to defaults.")
            elif args.dump:
                for l in range(pad._NUM_LAYERS):
                    name = pad.get_layer_name(l)
                    print(f"\nLayer {l}: {name}")
                    for k in range(pad._NUM_KEYS):
                        cfg = pad.get_key(l, k)
                        r, c = k // 4, k % 4
                        print(f"  K{k:02d}({r},{c}) "
                              f"'{cfg['label']:<8}' '{cfg['macro_str']}'")
            elif args.layer is not None and args.name is not None:
                pad.set_layer_name(args.layer, args.name)
                print(f"Layer {args.layer} name set to '{args.name}'")
            elif (args.layer is not None and args.key is not None
                  and args.label is not None and args.macro is not None):
                pad.set_key(args.layer, args.key, args.label, args.macro)
                print(f"L{args.layer} K{args.key} set: "
                      f"'{args.label}' → '{args.macro}'")
            elif args.layer is not None and args.key is not None:
                cfg = pad.get_key(args.layer, args.key)
                print(f"L{args.layer} K{args.key}: "
                      f"label='{cfg['label']}' macro='{cfg['macro_str']}'")
            else:
                parser.print_help()
    except OSError as e:
        print(f"Error: Could not connect: {e}")
        sys.exit(1)
    except ValueError as e:
        print(f"Error: {e}")
        sys.exit(1)


if __name__ == '__main__':
    main()
