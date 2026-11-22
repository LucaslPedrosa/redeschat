#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cache_dir="$project_dir/build/portable-ubuntu-22.04"
rootfs_dir="$cache_dir/rootfs"
rootfs_archive="$cache_dir/ubuntu-base-22.04.5-amd64.tar.gz"
rootfs_url="https://cdimage.ubuntu.com/ubuntu-base/releases/22.04/release/ubuntu-base-22.04.5-base-amd64.tar.gz"
output_dir="$cache_dir/output"
dependency_cache="$cache_dir/deps"

for command_name in bwrap curl tar; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'Erro: comando obrigatorio nao encontrado: %s\n' "$command_name" >&2
        exit 1
    fi
done

mkdir -p "$cache_dir" "$output_dir" "$dependency_cache"
if [[ ! -f "$rootfs_archive" ]]; then
    curl --fail --location --output "$rootfs_archive" "$rootfs_url"
fi

if [[ ! -x "$rootfs_dir/bin/bash" ]]; then
    rm -rf "$rootfs_dir"
    mkdir -p "$rootfs_dir"
    tar -xzf "$rootfs_archive" -C "$rootfs_dir"
fi

rm -rf "$output_dir"/*
bwrap_args=(
    --unshare-user --uid 0 --gid 0
    --bind "$rootfs_dir" /
    --dev-bind /dev /dev
    --proc /proc
    --ro-bind /etc/resolv.conf /etc/resolv.conf
    --ro-bind "$project_dir" /workspace
    --bind "$output_dir" /output
    --bind "$dependency_cache" /portable-deps
    --tmpfs /tmp
)
if [[ -d "$HOME/.xmake" ]]; then
    mkdir -p "$rootfs_dir/home/lucca/.xmake"
    bwrap_args+=(--bind "$HOME/.xmake" /home/lucca/.xmake)
fi
bwrap "${bwrap_args[@]}" /bin/bash /workspace/packaging/build-in-ubuntu.sh

mkdir -p "$project_dir/dist"
rm -rf "$project_dir/dist/RedesChat-linux-x86_64" \
    "$project_dir/dist/.package-work"
cp -f "$output_dir"/* "$project_dir/dist/"
printf 'Portateis disponiveis em %s\n' "$project_dir/dist"
