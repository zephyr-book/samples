# ZBook samples

Sample applications for the **ZBook** board (Raspberry Pi RP2350B, Cortex-M33),
built on Zephyr 4.4.2.

This repo is both the west manifest and the applications (a T2 topology
workspace), so it is all you need to bootstrap:

```bash
west init -m git@github.com:zephyr-book/samples.git zbook-samples
cd zbook-samples/samples
west update
```

That leaves:

```
zbook-samples/
├── samples/     # this repo: the apps + west.yml
├── zbook/       # board support, a Zephyr module (zephyr-book/zbook)
└── deps/        # west-managed: zephyr 4.4.2, hal_rpi_pico, cmsis_6,
                 #   lvgl, littlefs
```

The board target comes from the `zbook` module, which registers its own
`board_root`, so pass it with `-b` and never set `BOARD_ROOT`.

## The samples

| Sample | What it is |
| --- | --- |
| [`bringup_p2`](bringup_p2/) | Hardware bring-up for the **P2** revision: every on-board module exercised from an encoder-driven menu on the 128×64 OLED. LVGL 9, no host needed. |

## Building

There is a `justfile` at the root of this repo wrapping the usual west
commands. Run it from here:

```bash
just                  # list the samples
just run              # build + flash + serial monitor
just build bringup_p2
just run-wifi         # same app, with the ESP8266 shield screen
just run-debug        # same app, plus a shell on the console UART
just size             # ROM/RAM breakdown
just clean all
```

Each sample builds into its own tree under `build/<sample>`, so switching
between them never needs a pristine rebuild. `just` cds here on its own, so it
works from any subdirectory.

Or by hand:

```bash
west build -b zbook@p2/rp2350b/m33 -d build/bringup_p2 bringup_p2
west flash -d build/bringup_p2
```

### Flashing

`west flash` uses OpenOCD over the CMSIS-DAP probe on the "Prog USB-C" port
(the on-board RP2040 acts as the debug probe). RP2350 support is not in OpenOCD
0.12.0, so it needs the Raspberry Pi fork; the `justfile` points at a local
build of it, which you may need to adjust:

```
openocd_bin := "~/.local/bin/usr/local/bin/openocd"
openocd_dir := "~/.local/openocd"
```

The console is on **UART0 at 115200 baud**, bridged to the host by the same
probe. `just monitor` attaches `tio` to it, auto-detecting the port.

## Board revisions

The `zbook` module describes both the P1 and P2 revisions, selected by the
revision suffix on the board target:

```
-b zbook/rp2350b/m33        # P1
-b zbook@p2/rp2350b/m33     # P2 (what the samples here target)
```

P2 re-pins almost every peripheral and adds the rotary encoder, so a sample
that uses the encoder will not build for P1.

## Licence

Apache-2.0. See [LICENSE](LICENSE).
