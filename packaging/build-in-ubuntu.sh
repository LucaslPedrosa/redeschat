#!/usr/bin/env bash

set -euo pipefail
export DEBIAN_FRONTEND=noninteractive

export HOME=/root

if [[ ! -f /root/.redeschat-builder-v4 ]]; then
    apt-get -o APT::Sandbox::User=root update
    apt-get -o APT::Sandbox::User=root install -y --no-install-recommends \
        bash binutils build-essential ca-certificates curl file git meson \
        libdbus-1-3 libegl1 libexpat1-dev libffi-dev libfontconfig1 \
        libfontconfig1-dev libfreetype6 libfreetype6-dev libgl1 \
        libglib2.0-0 libssl-dev libx11-6 libx11-xcb1 libxau6 libxml2-dev \
        libxcb-cursor0 \
        libxcb-icccm4 libxcb-image0 libxcb-keysyms1 libxcb-randr0 \
        libxcb-render-util0 libxcb-render0 libxcb-shape0 libxcb-shm0 \
        libxcb-sync1 libxcb-util1 libxcb-xfixes0 libxcb-xkb1 libxcb1 \
        libxdmcp6 libxkbcommon-x11-0 libxkbcommon0 pkg-config python3 \
        ripgrep squashfs-tools tar unzip zlib1g-dev zip || true
    for required_file in /usr/bin/g++ /usr/bin/mksquashfs \
        /usr/lib/x86_64-linux-gnu/libfontconfig.so.1 \
        /usr/lib/x86_64-linux-gnu/libxcb-cursor.so.0; do
        if [[ ! -e "$required_file" ]]; then
            printf 'Erro: dependencia do ambiente ausente: %s\n' "$required_file" >&2
            exit 1
        fi
    done
    curl -fsSL https://xmake.io/shget.text | bash
    touch /root/.redeschat-builder-v4
fi

export PATH="/root/.local/bin:/root/.xmake/bin:$PATH"
export XMAKE_ROOT=y
export TAR_OPTIONS=--no-same-owner
if ! command -v xmake >/dev/null 2>&1; then
    printf '%s\n' 'Erro: instalacao do Xmake nao foi localizada.' >&2
    exit 1
fi

export HOME=/home/lucca
export REDESCHAT_DEPENDENCY_CACHE=/portable-deps
source_dir=/tmp/redeschat-source
mkdir -p "$source_dir"
tar -C /workspace --exclude=./build --exclude=./dist \
    -cf - . | tar -C "$source_dir" -xf -

cd "$source_dir"
chmod +x scripts/package-linux.sh
./scripts/package-linux.sh
cp -f dist/RedesChat-x86_64.AppImage \
    dist/RedesChat-x86_64.run \
    dist/RedesChat-linux-x86_64.zip /output/
