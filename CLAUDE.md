# CLAUDE.md

Guidance for Claude Code (claude.ai/code) working in this repository.

## What this is

Sample applications for the **ZBook** board (Raspberry Pi **RP2350B**,
Cortex-**M33**) on Zephyr **4.4.2**. This repo is *both* the west manifest and
the applications — a **T2 topology** workspace:

```
<workspace>/
├── samples/     # this repo: the apps + west.yml
├── zbook/       # board support, a Zephyr module (zephyr-book/zbook)
└── deps/        # west-managed: zephyr 4.4.2, hal_rpi_pico, cmsis_6,
                 #   lvgl, littlefs, fatfs
```

`west.yml` imports only the modules the samples build against, not all of
Zephyr's. **`fatfs` is not optional** even though nothing here touches the
microSD: the board's own `zbook_rp2350b_m33_defconfig` sets
`CONFIG_FILE_SYSTEM` and `CONFIG_FAT_FILESYSTEM_ELM`, so the zephyr tree
compiles `subsys/fs/fat_fs.c` and needs the module's `ff.h`.

## Working directory

**Run all build/flash/west commands from this directory.** `just` cds here on
its own, so it works from any subdirectory.

## Build & flash

The board target comes from the `zbook` module, which registers its own
`board_root` — pass it with `-b`, never set `BOARD_ROOT`. The **revision
suffix selects the hardware**: `zbook/rp2350b/m33` is P1,
`zbook@p2/rp2350b/m33` is P2. The samples here target **P2 only** (P1 has no
rotary encoder, and `main.c` stops the build with an `#error`).

```bash
just                  # list the samples
just run              # build + flash + tio on the console
just build bringup_p2
just rebuild          # -p always; needed after changing `board` in the justfile
just run-wifi         # + --shield zbook_wifi -S zbook-wifi-credentials-littlefs
just run-debug        # + debug.conf (shell on the console UART)
just size             # rom_report + ram_report
just clean all
```

- **OpenOCD**: RP2350 support is not in 0.12.0, so `west flash` needs the
  Raspberry Pi fork. The `justfile` points at a local build
  (`~/.local/bin/usr/local/bin/openocd`, search dir `~/.local/openocd`). If it
  fails to launch on a `libjim` dylib, the *installed* copy is stale — rebuild
  is not needed, just `make install DESTDIR=$HOME/.local/bin` from the source
  tree, which links the internal jimtcl submodule statically.
- Console is **UART0 at 115200**, bridged by the on-board RP2040 on the "Prog
  USB-C" port. `just monitor` attaches `tio`, auto-detecting the port.
- After editing `west.yml`, run `west update`.

## Samples

### `bringup_p2`

One application exercising every module on the P2 board, as an encoder-driven
menu on the 128x64 OLED. See its [README](bringup_p2/README.md).

**Structure.** `src/menu.c` holds the module table and the home screen;
`src/mod_*.c` is one file per hardware module, each exposing a
`struct bringup_module`; `src/oled_menu.c` is the menu layer (styles, screens,
rows, focus groups, title-bar clock); `src/clock_sync.c` sets the clock from
SNTP.

**Adding a module**: add `src/mod_<name>.c`, declare its `bringup_module` in
`include/module.h`, and list it in the table at the top of `src/menu.c`.
`CMakeLists.txt` globs `src/*.c` **with `CONFIGURE_DEPENDS`**, so new files are
picked up without a pristine build — a plain glob is evaluated once and cached,
which silently fails to link new files.

A module fills in `label`, `create()`, `screen()`, and optionally `refresh()`
for live values, `present()` to report whether the hardware is actually there,
and `is_showing()` if it owns more than one screen (a submenu).

**Absent hardware keeps its row, struck through** (`present()` returning false)
rather than being dropped from the menu. On a partly-populated board that is
the most useful thing the menu can say. Never hide a module because the part
might not be fitted.

## Gotchas worth not re-learning

- **`CONFIG_ASSERT=y` is required.** Zephyr maps LVGL's `LV_ASSERT_HANDLER`
  onto `__ASSERT_NO_MSG()`, which compiles away without it — including
  `LV_ASSERT_MALLOC`. Every screen is built up front, so an undersized
  `LV_Z_MEM_POOL_SIZE` runs out during startup, hands back a half-built
  widget, and faults later on a garbage style pointer with nothing logged.
- **Size the LVGL pool from a measurement**, not a guess: `lvgl stats memory`
  in the debug build. Currently 34 KiB used of 65536.
- **Title bar text must leave room for the clock** (`HH:MM`, 5 of the 16
  columns). Row text may use all 16.
- **ESP-AT offloads the IP stack and its own DHCP**, so Zephyr never learns the
  lease's DNS servers and every hostname lookup fails `-EAGAIN`. `wifi.conf`
  sets `CONFIG_DNS_SERVER1` statically. Check `net dns` before blaming a
  server or a timeout.
- **SNTP does not run on the system workqueue**: name resolution plus a socket
  overflows its 1 KiB stack (faulting with `Illegal load of EXC_RETURN into
  PC`), and a timeout would block a shared queue for seconds. `clock_sync.c`
  uses a dedicated queue with 4 KiB.
- **Wi-Fi credentials live in flash**, via the board's
  `zbook-wifi-credentials-littlefs` snippet (settings on LittleFS at `/lfs`).
  Store them once from the shell with `wifi cred add` / `wifi cred auto_connect`.
  **Never put an SSID or passphrase in the source tree**, and note
  `wifi cred list` prints the passphrase in plaintext.
- `file open error (-2)` on a fresh board is the settings file not existing
  yet. Harmless, and gone once credentials are stored.

## Known hardware defects (P2)

Do not spend time re-diagnosing these from software:

- **Addressable LED strip does not light** — zephyr-book/zbook#4. The MCU side
  is verified good: `LED_ADDR` is GPIO31, PIO1 drives the pad, and pixels
  injected straight into `PIO1 TXF0` over SWD are consumed.
- **Microphone reads nothing** — zephyr-book/zbook#5. `MICROPHONE_ADC`
  (GPIO40/ADC0) is AC-coupled through C48 with no resistor on the ADC side, so
  the node floats.

`TMP1075` and `BMI323` are simply **not populated** on the current board; the
menu strikes them through, which is correct behaviour, not a bug.

## Contributing

**Never push to `main`.** Work goes on a feature branch and through a PR:

```bash
git checkout -b <branch>
git push -u origin <branch>
gh pr create --base main
```

This repo is a west project, so it is normally checked out detached at the
manifest revision — the `checkout -b` is what gives you a branch to commit on.
Be aware that leaving the worktree on a branch means a later `west update`
checks `main` back out and takes un-merged work with it.

The same applies to the `zbook` board module. Do not put AI or tool
attribution in commit messages or PR bodies.

## C code style

`.clang-format` is Zephyr upstream: LLVM base, **tabs**, 8-wide indent,
**100-col** limit, Linux braces, `InsertBraces: true`. In addition:

- Use `/* */` and `/** */` (Doxygen) comments, never `//`.
- Always brace `if`/loops, even single-line bodies.
- Avoid `typedef`; be explicit (`struct foo`).

Run `clang-format -i` on touched files before committing.
