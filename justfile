# Build / flash / run the ZBook sample applications.
#
# Run from this directory (`just` cds here on its own). Every sample gets its
# own build tree under build/<sample>, so switching between them never needs a
# pristine rebuild.
#
#   just                  # list the samples
#   just run              # build + flash + serial monitor, default sample
#   just run bringup_p2
#   just build bringup_p2
#   just monitor port=/dev/cu.usbmodem14201
#   just run-wifi         # same app, with the ESP8266 shield screen
#   just run-debug        # same app, plus a shell on the console UART
#
# The LVGL samples need the `lvgl` module, which is pulled in by the workspace
# manifest -- run `just update` once if deps/modules/lib/gui/lvgl is missing.


alias b := build
alias f := flash
alias r := run
alias m := monitor
alias c := clean

# ZBook P2 hardware revision. P1 has no rotary encoder, so these samples will
# not build for it.
board := "zbook@p2/rp2350b/m33"

default_sample := "bringup_p2"

# Shell UART, bridged to the host by the on-board RP2040 on the "Prog USB-C"
# port.
baud := "115200"

# Empty = auto-detect the first /dev/cu.usbmodem*. Override with
# `just monitor port=/dev/cu.usbmodemXXXX`.
port := ""

# Homebrew's openocd predates RP2350 support, so the runner needs the locally
# built one (same paths the usb_ncm_net app uses).
openocd_bin := "~/.local/bin/usr/local/bin/openocd"
openocd_dir := "~/.local/openocd"

# List every sample in this tree.
default: list

list:
    @find . -name CMakeLists.txt -not -path './build/*' \
        | sed -e 's|^\./||' -e 's|/CMakeLists.txt$||' | sort

# Build one sample into build/<sample>.
build name=default_sample:
    west build -p auto -b {{ board }} -d build/{{ name }} {{ name }}

# Force a pristine (from-scratch) build. Needed after changing `board` above.
rebuild name=default_sample:
    west build -p always -b {{ board }} -d build/{{ name }} {{ name }}

# Flash an already-built sample over SWD (CMSIS-DAP probe on "Prog USB-C").
flash name=default_sample:
    west flash -d build/{{ name }} --openocd {{ openocd_bin }} --openocd-search {{ openocd_dir }}

# The same application with the Wi-Fi screen and the SNTP clock compiled in.
# Needs the ESP8266 shield on the H2 header; without it the Wi-Fi row is just
# struck through.
#
# The snippet stores the Wi-Fi credentials in flash (settings on LittleFS).
# Store them once, from the shell this build already has:
#
#     wifi cred add -s <SSID> -p <PSK> -k 1
#     wifi cred auto_connect
#
# After that the board associates on boot and sets its clock from SNTP, which
# is what puts the time in the title bar. The credentials survive reboots and
# reflashes, and never enter the source tree.
build-wifi name=default_sample:
    west build -p auto -b {{ board }} -d build/{{ name }}-wifi {{ name }} \
        --shield zbook_wifi -S zbook-wifi-credentials-littlefs \
        -- -DEXTRA_CONF_FILE=wifi.conf

flash-wifi name=default_sample:
    west flash -d build/{{ name }}-wifi --openocd {{ openocd_bin }} --openocd-search {{ openocd_dir }}

run-wifi name=default_sample: (build-wifi name) (flash-wifi name) monitor

# The same application plus an interactive shell on the console UART, for when
# the hardware misbehaves. See the sample's debug.conf for what it adds.
build-debug name=default_sample:
    west build -p auto -b {{ board }} -d build/{{ name }}-debug {{ name }} -- -DEXTRA_CONF_FILE=debug.conf

flash-debug name=default_sample:
    west flash -d build/{{ name }}-debug --openocd {{ openocd_bin }} --openocd-search {{ openocd_dir }}

run-debug name=default_sample: (build-debug name) (flash-debug name) monitor

# Build, flash, then attach the serial console: the full loop for one sample.
run name=default_sample: (build name) (flash name) monitor

# Attach `tio` to the board's shell/log UART.
monitor:
    #!/usr/bin/env bash
    set -euo pipefail
    tty="{{ port }}"
    if [[ -z "${tty}" ]]; then
        tty=$(ls -t /dev/cu.usbmodem* 2>/dev/null | head -n 1 || true)
    fi
    if [[ -z "${tty}" ]]; then
        echo "No /dev/cu.usbmodem* found. Plug the Prog USB-C cable in, or pass" >&2
        echo "  just monitor port=/dev/cu.usbmodemXXXX" >&2
        exit 1
    fi
    echo "--- ${tty} @ {{ baud }} baud (ctrl-t q to quit) ---"
    exec tio -b {{ baud }} "${tty}"

# Kconfig editors for one sample's build tree.
menuconfig name=default_sample:
    west build -d build/{{ name }} -t menuconfig

# ROM / RAM breakdown of a built sample.
size name=default_sample:
    west build -d build/{{ name }} -t rom_report
    west build -d build/{{ name }} -t ram_report

# Fetch/refresh the west modules the samples depend on (lvgl among them).
update:
    west update

# Drop one sample's build tree, or all of them with `just clean all`.
clean name=default_sample:
    #!/usr/bin/env bash
    set -euo pipefail
    if [[ "{{ name }}" == "all" ]]; then
        rm -rf build
    else
        rm -rf "build/{{ name }}"
    fi

# Erase the whole RP2350B flash. Use when a bad image bricks the boot.
erase:
    {{ openocd_bin }} -f interface/cmsis-dap.cfg -f target/rp2350.cfg \
        -c "init; reset halt; flash erase_sector 0 0 last; shutdown"
