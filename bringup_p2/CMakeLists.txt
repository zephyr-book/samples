# SPDX-License-Identifier: Apache-2.0
#
# Copyright (c) Centro de Inovacao EDGE

cmake_minimum_required(VERSION 3.20.0)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(bringup_p2)

# One test per on-board module, plus the shared menu layer, all in this one
# application. New src/mod_*.c files are picked up with no edit here --
# CONFIGURE_DEPENDS is what makes that true without a pristine build, since a
# plain glob is evaluated once and then cached.
file(GLOB app_sources CONFIGURE_DEPENDS src/*.c)
target_sources(app PRIVATE ${app_sources})

target_include_directories(app PRIVATE include)
