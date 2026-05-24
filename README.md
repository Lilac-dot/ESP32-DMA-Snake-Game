# ESP32 Snake — Bare-Metal Game Engine on ST7735 TFT

> A fully custom Snake game built on ESP32-WROOM using ESP-IDF, with a hand-written SPI display driver, RGB565 framebuffer, DMA transfers, FreeRTOS task architecture, and zero external graphics libraries.

---

## Table of Contents

- [Overview](#overview)
- [Why This Project Is Interesting](#why-this-project-is-interesting)
- [Technical Highlights](#technical-highlights)
- [Features](#features)
- [Hardware](#hardware)
- [Software Stack](#software-stack)
- [System Architecture](#system-architecture)
- [How the Framebuffer Works](#how-the-framebuffer-works)
- [DMA Rendering Pipeline](#dma-rendering-pipeline)
- [Display Driver Internals](#display-driver-internals)
- [Game Logic](#game-logic)
- [Performance Optimizations](#performance-optimizations)
- [Wiring](#wiring)
- [Project Structure](#project-structure)
- [Build Instructions](#build-instructions)
- [Flash & Monitor](#flash--monitor)
- [Known Limitations & Future Improvements](#known-limitations--future-improvements)
- [Screenshots](#screenshots)
- [Skills Demonstrated](#skills-demonstrated)
- [Resume Description](#resume-description)
- [Elevator Pitch](#elevator-pitch)
- [License](#license)

---

## Overview

This project implements a complete Snake game on an **ESP32-WROOM** microcontroller driving a **1.8" ST7735 SPI TFT display** — entirely without any graphics abstraction libraries such as TFT_eSPI, Adafruit GFX, or LVGL.

Every layer of the display pipeline — from the SPI initialization sequences, to the framebuffer layout in SRAM, to the DMA transfer engine — is written from scratch using the **ESP-IDF framework** directly. The game runs inside a **FreeRTOS task** with a state machine managing menu, gameplay, and game-over screens, while a custom incremental rendering engine minimizes pixel writes per frame.

This is not a tutorial project. It is a systems-level implementation of a real-time embedded graphics engine.

---

## Why This Project Is Interesting

Most embedded game projects for microcontrollers rely on opaque graphics libraries that hide every interesting detail. `TFT_eSPI::fillRect()` is a black box. You call it, pixels appear, and you learn nothing about SPI protocols, pixel formats, memory layouts, or DMA.

This project strips all of that away.

**Every concept here maps directly to professional embedded graphics work:**

- The RGB565 framebuffer is the same format used in Linux framebuffer drivers and embedded display controllers.
- The `spi_device_queue_trans` / `spi_device_get_trans_result` pattern is the same double-buffered DMA pipeline used in production displays on industrial HMI panels.
- The `setAddrWindow` + `RAMWR` command sequence is the foundation of every SPI display driver ever written for the ST77xx family — from hobbyist screens to small-run consumer electronics.
- The FreeRTOS task architecture with microsecond-precision game tick timing reflects how production embedded game loops and UI refresh cycles are structured.
- Writing your own pixel font renderer — even a simple one — forces you to understand bitmapped glyph storage, column-major byte packing, and bitfield extraction.

The project demonstrates not just that you can *use* a microcontroller, but that you understand *what the microcontroller is doing*.

---

## Technical Highlights

| Area | Implementation |
|---|---|
| Display protocol | Raw 4-wire SPI via `spi_device_transmit` / `spi_device_queue_trans` |
| Pixel format | RGB565 (16-bit, 5R-6G-5B), big-endian per ST7735 spec |
| Framebuffer | 128×160×2 = 40,960 bytes in SRAM |
| DMA transfer | Full-frame async DMA via `SPI_DMA_CH_AUTO`, 27 MHz SPI clock |
| Graphics primitives | `drawPixel`, `fillRect`, `fillScreen` — all custom |
| Font rendering | Column-major 5×7 bitmapped font, bitfield extraction per column |
| Rendering strategy | Incremental: only changed cells (head + tail) redrawn per tick |
| Game loop | FreeRTOS pinned task, microsecond timer via `esp_timer_get_time()` |
| Input | GPIO with hardware pull-ups, active-low debounce via state check |
| Performance logging | Per-frame FPS measurement, serial UART debug output |
| State machine | `MENU → PLAYING → GAME_OVER` with clean transitions |

---

## Features

### Display Engine
- Custom ST7735 initialization sequence (software reset, sleep-out, color mode, MADCTL, display-on)
- Direct SPI command/data protocol with DC pin control
- `setAddrWindow()` implementing CASET + RASET + RAMWR for arbitrary rectangular pixel writes
- Full-screen RGB565 framebuffer rendered via async DMA SPI transfer
- Custom graphics primitives: pixel, filled rectangle, full-screen fill

### Font & Text
- 5×7 column-major bitmapped font (A–Z, space)
- `drawChar()` → `drawString()` → `drawNumber()` rendering pipeline
- Per-character bitfield extraction across 5 columns × 7 rows

### Game
- Classic Snake gameplay on a 16×20 tile grid (8px per cell)
- Smooth directional control with 180° reversal prevention
- Collision detection: wall boundary + self-intersection
- Food spawning with `rand()`-seeded placement
- Score counter and persistent high score across rounds
- Progressive speed increase: move interval decreases from 120ms to 50ms as score grows
- Three game states: main menu, active game, game-over overlay

### Performance
- Incremental rendering: only the snake head and vacated tail cell are repainted per game tick
- 27 MHz SPI clock for full-frame DMA push in ~1.2ms
- Per-frame FPS measurement with 30-frame serial reporting interval
- `vTaskDelay(pdMS_TO_TICKS(16))` yielding for ~60 Hz render loop cadence

---

## Hardware

| Component | Details |
|---|---|
| MCU | ESP32-WROOM-32 (Xtensa LX6 dual-core, 240 MHz, 520 KB SRAM) |
| Display | 1.8" ST7735S TFT, 128×160 pixels, 18-bit color (configured for 16-bit RGB565) |
| Interface | 4-wire SPI (MOSI, SCLK, CS, DC) + RST |
| Buttons | 4× tactile switches (UP, DOWN, LEFT, RIGHT) |
| Power | 3.3V logic, USB-powered via dev board |

---

## Software Stack

| Layer | Technology |
|---|---|
| Framework | ESP-IDF v5.x (CMake-based) |
| RTOS | FreeRTOS (bundled with ESP-IDF) |
| Language | C++ (extern "C" entry point for ESP-IDF compatibility) |
| SPI driver | ESP-IDF `driver/spi_master.h` |
| GPIO driver | ESP-IDF `driver/gpio.h` |
| Timer | `esp_timer_get_time()` — 64-bit microsecond hardware timer |
| Build system | CMake + `idf.py` |
| Debug output | UART via `printf` / ESP-IDF serial monitor |

No Arduino core. No TFT_eSPI. No Adafruit GFX. No LVGL. No display libraries of any kind.

---

## System Architecture

```
┌─────────────────────────────────────────────────────┐
│                    app_main()                        │
│  GPIO config → SPI bus init → Display init          │
│  → xTaskCreatePinnedToCore(gameTask, core 1)        │
└─────────────────────┬───────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│              gameTask() — FreeRTOS Task              │
│              Pinned to Core 1, Priority 1            │
│                                                      │
│  ┌──────────┐   ┌───────────┐   ┌───────────────┐  │
│  │   MENU   │──▶│  PLAYING  │──▶│  GAME_OVER    │  │
│  └──────────┘   └───────────┘   └───────────────┘  │
│       │              │                  │            │
│  drawMenu()    handleInput()       drawGame()        │
│  pushFB()      moveSnake()         pushFB()          │
│                drawGame()          anyBtn → reset    │
│                pushFB()                              │
└─────────────────────────────────────────────────────┘
                      │
                      ▼
┌─────────────────────────────────────────────────────┐
│              Rendering Pipeline                      │
│                                                      │
│  Game state changes                                  │
│       │                                              │
│       ▼                                              │
│  fillRect / drawPixel → framebuffer[y*128+x]        │
│       │  (SRAM write, 40,960 bytes)                  │
│       ▼                                              │
│  pushFramebuffer()                                   │
│       │                                              │
│       ▼                                              │
│  setAddrWindow(0,0,127,159)                          │
│  CASET + RASET + RAMWR commands via SPI              │
│       │                                              │
│       ▼                                              │
│  spi_device_queue_trans() ──▶ DMA engine             │
│  spi_device_get_trans_result() ◀── transfer done    │
│       │                                              │
│       ▼                                              │
│  ST7735 display — 128×160 pixels updated             │
└─────────────────────────────────────────────────────┘
```

---

## How the Framebuffer Works

The framebuffer is a flat array of `uint16_t` values in SRAM:

```c
uint16_t framebuffer[TFT_WIDTH * TFT_HEIGHT];  // 128 × 160 × 2 = 40,960 bytes
```

Each element stores one pixel in **RGB565** format:
- Bits 15–11: Red (5 bits)
- Bits 10–5: Green (6 bits)
- Bits 4–0: Blue (5 bits)

Pixel at display coordinate `(x, y)` maps to `framebuffer[y * TFT_WIDTH + x]`. This is a row-major linear layout matching the ST7735's RAMWR auto-increment direction.

All drawing operations (drawPixel, fillRect, fillScreen) write directly to this SRAM buffer. The display is never touched until `pushFramebuffer()` is explicitly called, which sends the entire buffer to the ST7735 in one DMA transaction. This is a **software framebuffer** pattern — common in embedded graphics, Linux `/dev/fb0`, and retro game consoles.

Color constants are defined as pre-computed RGB565 values:

```c
#define COLOR_BG      0x001F  // Blue
#define COLOR_SNAKE   0xFFE0  // Yellow
#define COLOR_FOOD    0xFFFF  // White
#define COLOR_BORDER  0xF81F  // Magenta
```

---

## DMA Rendering Pipeline

The full-frame push to the ST7735 uses ESP-IDF's asynchronous SPI DMA API:

```c
void pushFramebuffer() {
    // 1. Set the display's write window to cover full screen
    setAddrWindow(0, 0, TFT_WIDTH - 1, TFT_HEIGHT - 1);

    // 2. Assert DC high (data mode, not command mode)
    gpio_set_level(TFT_DC, 1);

    // 3. Enqueue a DMA SPI transaction for the entire framebuffer
    spi_transaction_t trans = {};
    trans.length    = TFT_WIDTH * TFT_HEIGHT * 16;  // bits
    trans.tx_buffer = framebuffer;
    spi_device_queue_trans(spi, &trans, portMAX_DELAY);

    // 4. Block until DMA engine completes the transfer
    spi_transaction_t *rtrans;
    spi_device_get_trans_result(spi, &rtrans, portMAX_DELAY);
}
```

At 27 MHz SPI with 16-bit pixels:
- Total bits: 128 × 160 × 16 = **327,680 bits**
- Transfer time: 327,680 / 27,000,000 ≈ **~12.1ms** per full frame push

The `spi_device_queue_trans` / `spi_device_get_trans_result` split is the canonical ESP-IDF pattern for DMA-backed SPI — it allows the CPU to do other work while the DMA engine feeds the SPI peripheral from SRAM, freeing the bus for concurrent operations in multi-device configurations.

`setAddrWindow()` implements the standard ST77xx address window protocol:
1. `CASET (0x2A)` — sets column start/end (X axis)
2. `RASET (0x2B)` — sets row start/end (Y axis)
3. `RAMWR (0x2C)` — opens the pixel write stream; subsequent data bytes fill pixels left-to-right, top-to-bottom

---

## Display Driver Internals

The ST7735 initialization sequence configures the panel for 16-bit RGB565 color, the correct scan direction, and display-on state:

```c
sendCommand(0x01);  // SWRESET — software reset
vTaskDelay(150ms);
sendCommand(0x11);  // SLPOUT — exit sleep mode
vTaskDelay(150ms);
sendCommand(0x3A);  // COLMOD — pixel format
sendData(0x05);     //   → 16-bit/pixel RGB565
sendCommand(0x36);  // MADCTL — memory access control
sendData(0xC8);     //   → MY=1, MX=1, BGR=1 (180° + BGR order)
sendCommand(0x29);  // DISPON — display on
```

The DC (Data/Command) pin distinguishes register writes from pixel data — a fundamental aspect of the ST77xx protocol that every SPI display driver must handle manually.

---

## Game Logic

### Grid System
The display is divided into an 8×8 pixel tile grid: 16 columns × 20 rows. The snake and food are tracked in grid coordinates; pixel coordinates are computed as `grid_x * GRID_SIZE` for rendering.

### Snake Representation
The snake is stored as an array of `Point` structs (grid coordinates), head at index 0:
```c
Point snake[100];  // max 100 segments
int snakeLength;   // current active length
```

### Movement
Each game tick (controlled by `moveDelay` microsecond timer):
1. Compute new head position from current direction
2. Check wall collision: `x < 0 || x >= GRID_W || y < 0 || y >= GRID_H`
3. Check self-collision: scan all `snakeLength` body segments
4. Shift body array: `snake[i] = snake[i-1]` for all i
5. Write new head: `snake[0] = newHead`

### Incremental Rendering
Only two cells change per tick:
- **Old tail** → erased with `COLOR_BG`
- **New head** → drawn with `COLOR_SNAKE`

This avoids repainting the entire snake body every frame, saving hundreds of pixel writes per tick.

### Speed Progression
```c
moveDelay = 120000;  // 120ms initial (≈8.3 ticks/sec)

// On each food eaten:
if (moveDelay > 50000)
    moveDelay -= 3000;  // floor at 50ms (20 ticks/sec)
```
Maximum speed is reached after approximately 23 food items consumed.

---

## Performance Optimizations

1. **Incremental rendering**: Only the head and tail cells are repainted per game tick — not the entire snake or screen. This is a simplified dirty-region approach reducing SRAM writes proportional to snake length.

2. **DMA SPI transfer**: The framebuffer is sent via ESP-IDF's DMA-backed SPI engine rather than byte-by-byte CPU-driven transfers, keeping the bus throughput near the theoretical maximum of 27 MHz.

3. **Pre-computed color constants**: RGB565 color values are `#define` constants, computed at compile time — no runtime color conversion.

4. **Static SPI transaction struct**: The `spi_transaction_t` in `pushFramebuffer()` is declared `static` to avoid stack allocation on every call.

5. **Single-task architecture**: All rendering, input, and game logic share one FreeRTOS task on core 1, eliminating inter-task synchronization overhead for the current scope.

6. **Pinned task**: `xTaskCreatePinnedToCore(..., 1)` pins the game loop to core 1, keeping core 0 free for Wi-Fi/BT stacks (if added) and the FreeRTOS idle task.

---

## Wiring

| ST7735 TFT Pin | ESP32 GPIO | Description |
|---|---|---|
| VCC | 3.3V | Power |
| GND | GND | Ground |
| CS | GPIO 5 | SPI Chip Select |
| RESET | GPIO 4 | Display Hardware Reset |
| DC/RS | GPIO 2 | Data / Command select |
| MOSI/SDA | GPIO 23 | SPI MOSI |
| SCK/CLK | GPIO 18 | SPI Clock |
| LED/BL | 3.3V | Backlight (always on) |

| Button | ESP32 GPIO | Direction |
|---|---|---|
| UP | GPIO 32 | Active-low, pull-up enabled |
| DOWN | GPIO 33 | Active-low, pull-up enabled |
| LEFT | GPIO 25 | Active-low, pull-up enabled |
| RIGHT | GPIO 26 | Active-low, pull-up enabled |

---

## Project Structure

```
esp32-snake/
├── main/
│   ├── CMakeLists.txt
│   └── main.cpp          # All source: driver, engine, game logic
├── CMakeLists.txt         # Top-level ESP-IDF project CMake
├── sdkconfig              # ESP-IDF menuconfig output (generated)
├── partitions.csv         # Optional custom partition table
└── README.md
```

---

## Build Instructions

### Prerequisites

- ESP-IDF v5.x installed and sourced (`source ~/esp/esp-idf/export.sh`)
- ESP32-WROOM target board connected via USB
- CMake ≥ 3.16, Python ≥ 3.8

### Build

```bash
git clone https://github.com/yourusername/esp32-snake.git
cd esp32-snake

# Set target (ESP32, not ESP32-S2/S3/C3)
idf.py set-target esp32

# (Optional) Configure via menuconfig
idf.py menuconfig

# Build
idf.py build
```

---

## Flash & Monitor

```bash
# Flash to device (replace PORT with your serial port)
idf.py -p /dev/ttyUSB0 flash

# Monitor serial output (FPS, score, high score)
idf.py -p /dev/ttyUSB0 monitor

# Combined: flash then immediately open monitor
idf.py -p /dev/ttyUSB0 flash monitor
```

Expected serial output during gameplay:
```
FPS: 60.00 | Score: 3 | HighScore: 5 | SnakeLength: 6
FPS: 61.00 | Score: 3 | HighScore: 5 | SnakeLength: 6
```

Press `Ctrl+]` to exit the serial monitor.

---

## Known Limitations & Future Improvements

### Current Limitations

- **Number rendering**: The `drawChar()` function renders digit characters (0–9) as solid filled blocks rather than distinct glyph shapes. A proper 5×7 numeric glyph table is needed.
- **Food spawn collision**: `spawnFood()` uses `rand() % GRID_W/H` with no check against snake body — food can technically spawn inside the snake on a long game.
- **Snake array cap**: `snake[100]` limits maximum length to 100 segments, roughly 31% of the 16×20 grid. Should be `GRID_W * GRID_H = 320`.
- **Display init**: The ST7735 initialization omits panel-specific registers (FRMCTR1, PWCTR, VMCTR, gamma tables). The minimal sequence works but color accuracy and refresh rate are not optimal.
- **Border/play field overlap**: The 1-pixel border is drawn at pixels 0 and 127/159, but the snake grid starts at grid x=0 (pixel 0), meaning the snake can visually overlap the border.
- **No NVS persistence**: High score resets on power cycle. ESP-IDF NVS (non-volatile storage) API could persist it to flash.

### Planned Improvements

- [ ] Proper 5×7 numeric glyph table for score display
- [ ] Food spawn with snake-body exclusion check
- [ ] NVS high score persistence across resets
- [ ] Double-buffered rendering to eliminate potential tearing
- [ ] Sound via PWM buzzer (LEDC peripheral) on food eat / game over
- [ ] MADCTL-based display orientation option
- [ ] Full ST7735 init sequence with gamma correction registers
- [ ] Pause functionality (long press any button)
- [ ] Difficulty selection on menu screen
- [ ] Wi-Fi OTA update support for firmware without reflashing

---

## Screenshots

> *Add photos/screenshots of the hardware here.*

| Menu Screen | Gameplay | Game Over |
|---|---|---|
| ![menu](docs/menu.jpg) | ![gameplay](docs/gameplay.jpg) | ![gameover](docs/gameover.jpg) |

---

## Skills Demonstrated

### Embedded Systems
- Raw SPI peripheral configuration using ESP-IDF HAL (not Arduino wrappers)
- ST77xx display driver protocol: CASET, RASET, RAMWR, MADCTL, COLMOD command sequences
- DMA-backed SPI transactions via ESP-IDF `spi_device_queue_trans` / `spi_device_get_trans_result`
- GPIO configuration with hardware pull-up enable for active-low button inputs
- Microsecond-precision timing using ESP-IDF hardware timer (`esp_timer_get_time`)
- FreeRTOS task creation with core affinity pinning

### Graphics & Rendering
- RGB565 pixel format — endianness, bit packing, color constant derivation
- Row-major framebuffer layout in SRAM
- Column-major bitmapped font storage and per-bit glyph rendering
- Incremental / dirty-region rendering to minimize memory bus traffic
- Full-frame framebuffer DMA flush pipeline

### Systems Programming
- Memory-mapped framebuffer pattern (mirrors Linux `/dev/fb0` concepts)
- Fixed-size array game state with pointer-arithmetic-free index management
- Finite state machine: clean enum-based state transitions
- Stack-allocation avoidance for performance-critical paths (static `spi_transaction_t`)

### Software Engineering
- ESP-IDF CMake project structure
- `extern "C"` ABI boundary for C++ / C interop in ESP-IDF entry point
- Serial UART telemetry for embedded performance profiling
- Clean separation of driver layer, rendering layer, and game logic layer

---

## Resume Description

**ESP32 Snake Game — Bare-Metal Graphics Engine** | C++, ESP-IDF, FreeRTOS

Built a real-time Snake game on ESP32-WROOM with a custom ST7735 SPI display driver, RGB565 framebuffer, and DMA rendering pipeline — using ESP-IDF directly with no external graphics libraries. Implemented the full display stack from SPI initialization commands and address window protocol through framebuffer memory layout and async DMA transfers. Designed a FreeRTOS task-based game loop with microsecond-precision timing, incremental dirty-region rendering, and a bitmapped font engine. Achieved ~60 FPS render cadence with a 27 MHz DMA SPI pipeline pushing 40,960-byte framebuffer updates.

---

## Elevator Pitch

> "I built a Snake game on an ESP32, but the interesting part isn't the game — it's that I wrote the entire graphics stack from scratch. No TFT library, no Adafruit GFX, nothing. I'm talking raw SPI command sequences to initialize the ST7735 panel, a 40KB RGB565 framebuffer in SRAM, and DMA transfers to push it to the display. I built my own pixel renderer, my own font engine, my own dirty-region update system. The game runs inside a FreeRTOS task with microsecond game tick timing. It taught me everything about how displays actually work at the hardware level — which is directly applicable to any role working on embedded HMI, display controllers, or firmware for consumer electronics."

---

## License

MIT License. See [LICENSE](LICENSE) for details.
