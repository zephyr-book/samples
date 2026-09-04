# bringup_p2 — ZBook P2 hardware bring-up

One application that exercises every module on the ZBook **P2**, driven entirely
from the board itself: an encoder-turned menu on the on-board 128×64 OLED. No
host, no shell, no serial console needed — plug the board in and turn the knob.

```
┌──────────────────────────┐
│ ZBook P2           12:04 │  ← inverted title bar, clock at the right
│ > LEDs                   │  ← focus = full inversion
│   Buttons/Enc            │
│   RGB strip              │
│   Buzzer                 │
│   Pot/LDR/Mic            │
└──────────────────────────┘
   turn encoder → move focus
   press encoder → select
```

The clock appears once the board has been online and set its time from SNTP;
until then the title bar carries only the title, rather than a placeholder.
See [Wi-Fi and the clock](#wi-fi-and-the-clock).

Hardware that does not answer keeps its row, **struck through**, and its screen
says why. On a partly-populated board that is the single most useful thing the
menu can tell you, so nothing is ever silently left out of the list.

## The modules

| Row | What it tests | How |
| --- | --- | --- |
| `LEDs` | `led0`…`led3` + the white LED (`LAMP` on the silkscreen) | toggle each, or all on / all off |
| `Buttons/Enc` | 4 user buttons, encoder rotation and its button | live state, straight off the input subsystem |
| `RGB strip` | 4× FZ2812-5050 on PIO1 | R/G/B/white/off presets, plus a chase that proves the wiring order |
| `Buzzer` | piezo on PWM slice 8 | four tones, a sweep, and silence |
| `Pot/LDR/Mic` | ADC channels 1, 4 and 0 | all three live, raw counts and % of full scale |
| `Sense lines` | hall sensor, TMP1075 ALERT, microSD card-detect | live logical state |
| `Infrared` | IR emitter and receiver | steady on/off, plus a bit-banged 38 kHz burst |
| `Temperature` | TMP1075 on i2c0 | live °C and a bar |
| `Accelerometer` | BMI323 on spi1 | live X/Y/Z plus the INT2 line |
| `Display` | the OLED itself | reported geometry, contrast steps, blanking |
| `Wi-Fi` | ESP8266 on the H2 header | interface state and an AP scan; also what sets the clock |
| `About` | build identification | board target, Zephyr and LVGL versions |

Deliberately **not** covered: the microSD filesystem and the motor MOSFET. The
card-detect line is checked under `Sense lines`, but nothing is mounted, and
nothing here spins a motor.

## Build and run

From the `samples/` directory:

```bash
just run              # build + flash + serial monitor
just build            # build only
just flash            # flash an existing build
just size             # ROM/RAM breakdown

just run-wifi         # same app, with the Wi-Fi screen (needs the shield)
just run-debug        # same app, plus a shell on the console UART
```

Or by hand:

```bash
west build -b zbook@p2/rp2350b/m33 -d build/bringup_p2 samples/bringup_p2
west flash -d build/bringup_p2
```

**P2 only.** P1 has no rotary encoder, so the build stops with an explicit
`#error`.

LVGL comes from the `lvgl` west module; run `west update` (or `just update`) if
`deps/modules/lib/gui/lvgl` is missing.

### Wi-Fi and the clock

The Wi-Fi screen compiles into this same application, but the ESP8266 lives on
an attachable shield, so it needs its own build:

```bash
just run-wifi
```

which is `--shield zbook_wifi -S zbook-wifi-credentials-littlefs` plus
`wifi.conf`. Store the credentials once, from the shell this build already has:

```
wifi cred add -s <SSID> -p <PASSPHRASE> -k 1    # -k 1 = WPA2-PSK
wifi cred auto_connect
wifi cred list                                   # confirm
```

They go into flash — settings on LittleFS at `/lfs`, on the board's `storage`
partition — so they survive a reboot *and* a reflash of the application, and
they never enter the source tree. `file open error (-2)` on the very first boot
is just the settings file not existing yet.

After that the board associates on boot on its own, and once it is online it
queries SNTP and sets the wall clock; the title bar then shows `HH:MM` on every
screen. There is no RTC on this board, so the clock is the kernel's monotonic
time plus the offset SNTP established — it survives losing the link but not a
reset, and it re-syncs hourly to stay honest.

Four things are tunable in `Kconfig`: `BRINGUP_SNTP_SERVER`,
`BRINGUP_SNTP_TIMEOUT_MS`, `BRINGUP_SNTP_RESYNC_MINUTES` and
`BRINGUP_UTC_OFFSET_MINUTES`. That last one defaults to `-180` (UTC-3,
Brasilia) — there is no timezone database here, just a fixed offset, so change
it for your location.

#### Two traps worth knowing about

**A DNS server has to be configured statically.** The ESP8266 offloads the
whole IP stack and runs its own DHCP client, so the lease is negotiated inside
the module: Zephyr learns the address and gateway, but never the DNS servers.
`net dns` then lists nothing, every name lookup fails with `-EAGAIN`, and SNTP
reports that as "no time". The `esp_at` driver offloads sockets but *not*
`getaddrinfo`, so nothing else covers for it. Hence `CONFIG_DNS_SERVER1` in
`wifi.conf`, pointing at a public resolver so it works on any network.

**SNTP does not run on the system workqueue.** Name resolution plus a socket
needs far more stack than that queue's 1 KiB default — it overflows and faults
with `Illegal load of EXC_RETURN into PC`, a corrupted return address rather
than anything that names the real cause. And a query that times out blocks its
thread for the entire timeout, which is not something to inflict on a queue the
rest of the system shares. `clock_sync.c` therefore runs it on a dedicated
work queue with a 4 KiB stack.

Without the shield the row is struck through and its screen says the shield is
not in the build. With the shield fitted but the module not answering, the
ESP-AT driver fails its probe and the row is struck through too — the check is
`device_is_ready()` on the `esp8266` node, so it tests the hardware and not
just the devicetree.

`Scan` counts APs and reports the strongest RSSI. The count comes from
`net_mgmt` events rather than from the request, so a scan started anywhere else
— `wifi scan` on the net shell, say — is picked up and displayed too.

The interface reads `down` until it associates with a network; that is normal,
and no obstacle to scanning. There are no credentials in this build. To add
them, compose the upstream snippet: `--shield zbook_wifi -S wifi-credentials`.

Note the shield's `Kconfig.defconfig` turns on `NET_SHELL` and
`NET_L2_WIFI_SHELL`, so **this variant has a shell on the console UART even
without `debug.conf`** — handy, but it means the Wi-Fi build is not directly
comparable to the base build's footprint.

### When something misbehaves

`just run-debug` adds an interactive shell without changing any of the above:

```
i2c scan i2c0        # ssd1306 answers at 0x3c, tmp1075 would at 0x48
device list          # every driver's probe result in one place
input dump on        # raw input events from the buttons and encoder
lvgl stats memory    # LVGL heap use -- check before trimming the pool
cfb init && cfb invert
                     # drives the panel with display_write() directly, so a
                     # blank screen here means panel or driver, and a blank
                     # screen only under LVGL means LVGL
```

## Layout of the code

| File | Contents |
| --- | --- |
| `src/main.c` | display readiness check, then the `lv_timer_handler()` loop |
| `src/menu.c` | the module table, the home screen, and the refresh timer |
| `src/mod_*.c` | one file per module, each exposing a `struct bringup_module` |
| `src/oled_menu.c` | the menu layer: styles, screens, rows, focus groups, title-bar clock |
| `src/clock_sync.c` | SNTP sync on going online; a no-op without networking |
| `include/module.h` | the interface every module implements |
| `app.overlay` | the LVGL encoder input pseudo-device |
| `prj.conf` | drivers and LVGL configuration |
| `wifi.conf` | stored-credential connect and SNTP, for the shield build |
| `debug.conf` | the shell overlay above |
| `Kconfig` | SNTP server, timeout, re-sync period, UTC offset |

Adding a module means adding one `src/mod_<name>.c`, declaring its
`bringup_module` in `include/module.h`, and listing it in the table at the top
of `src/menu.c`. `CMakeLists.txt` globs `src/*.c`, so it needs no edit.

A module fills in four things: a `label` for its row, `create()` to build its
screen, `screen()` to hand it back, and optionally `refresh()` for live values
and `present()` to report whether the hardware is actually there.

## Notes on driving LVGL at 1 bpp

- `CONFIG_LV_COLOR_DEPTH_1` selects `LV_USE_THEME_MONO` automatically, so
  nothing here calls a theme init function.
- The SSD1306 is column-major with 8 rows per page. Zephyr's LVGL glue
  (`lvgl_display_mono.c`) rounds every redraw area to those page boundaries and
  packs the bits, so application code never sees the tiling.
- `CONFIG_LV_Z_COLOR_MONO_HW_INVERSION=y` — the controller inverts in hardware,
  so LVGL skips doing it in software.
- `CONFIG_LV_Z_VDB_SIZE=100` renders the whole screen at once. A 128×64 1 bpp
  frame is 1 KiB, so there is no reason to render it in tiles.
- The UNSCII 8 font is a 1 bpp bitmap font with no anti-aliasing to threshold
  away, and it makes the panel an exact 16×8 character grid — which is what the
  9 px title bar and 10 px rows in `oled_menu.c` are sized against.
- On a 1 bpp panel the mono theme's 1 px focus outline is easy to miss, so rows
  override `LV_STATE_FOCUSED` with a full inversion instead.
- **`CONFIG_ASSERT=y` matters here.** Zephyr maps LVGL's `LV_ASSERT_HANDLER`
  onto `__ASSERT_NO_MSG()`, which compiles away without it — including
  `LV_ASSERT_MALLOC`. Every screen is built up front, so an undersized
  `LV_Z_MEM_POOL_SIZE` runs out during startup, hands back a half-built widget,
  and faults later on a garbage style pointer with nothing logged.
