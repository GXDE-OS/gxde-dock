#!/bin/bash

# Copyright (C) 2026 CharOfString <markus_verify@126.com>
#
# This file is part of gxde-dock.
#
# gxde-dock is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# gxde-dock is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with gxde-dock.  If not, see <https://www.gnu.org/licenses/>.

# Rescan Wayland protocols and generate Python bindings using pywayland.
# Be sure to run "sudo apt install python3-pywayland python3-cffi" first!!

set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
OUT_DIR="$HERE/fallback_service/dock/protocols"

sanitize_path() {
    local raw="$1"

    # Remove " in the front
    raw="${raw#\"}"

    # Remove " in the back
    raw="${raw%\"}"

    # Remove ' in the front
    raw="${raw#\'}"

    # Remove ' in the back
    raw="${raw%\'}"

    # Remove trailing "/"
    raw="${raw%/}"
    echo "$raw"
}

echo "Ensuring dependencies..."
sudo apt install python3 python3-pip python3-pywayland python3-cffi wayland-protocols

if [ -n "$1" ]; then
    PROTOCOL_DIR="$(sanitize_path "$1")"
else
    echo "Please enter the gxde-wlcom protocol directory path."
    echo "That one contains all kywc-*.xml files."
    read -rp "DIR> " PROTOCOL_DIR
    PROTOCOL_DIR="$(sanitize_path "$PROTOCOL_DIR")"
fi

if [ ! -d "$PROTOCOL_DIR" ]; then
    echo "FATAL: INVALID DIRECTORY -> $PROTOCOL_DIR"
    exit 1
fi

mkdir -p "$OUT_DIR"

echo "Generating core protocol bindings..."
python3 -m pywayland.scanner --with-protocols -o "$OUT_DIR"

echo "Generating custom protocol bindings from $PROTOCOL_DIR..."
WAYLAND_CORE="/usr/share/wayland/wayland.xml"
shopt -s nullglob
kywc_files=("$PROTOCOL_DIR"/kywc-*.xml)
if [ ${#kywc_files[@]} -gt 0 ]; then
    python3 -m pywayland.scanner -i "$WAYLAND_CORE" "${kywc_files[@]}" -o "$OUT_DIR"
else
    echo "No kywc-*.xml found in $PROTOCOL_DIR"
fi
shopt -u nullglob

echo "Your protocol bindings are ready on: $OUT_DIR!!"
echo "You may now inspect: "
ls "$OUT_DIR"

echo "Bye!!"
