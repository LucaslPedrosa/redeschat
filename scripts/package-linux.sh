#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
build_file="$project_dir/build/linux/x86_64/release/redeschat"
dist_dir="$project_dir/dist"
package_name="RedesChat-linux-x86_64"
package_dir="$dist_dir/$package_name"
archive="$dist_dir/$package_name.zip"
appimage="$dist_dir/RedesChat-x86_64.AppImage"
run_file="$dist_dir/RedesChat-x86_64.run"
work_dir="$dist_dir/.package-work"
dependency_cache="${REDESCHAT_DEPENDENCY_CACHE:-$project_dir/build/portable-deps}"

for command_name in apt-get curl dpkg-deb ldd mksquashfs objdump rg tar xmake zip; do
    if ! command -v "$command_name" >/dev/null 2>&1; then
        printf 'Erro: comando obrigatorio nao encontrado: %s\n' "$command_name" >&2
        exit 1
    fi
done

cd "$project_dir"
xmake f -m release -y
xmake build redeschat

runtime_paths="$(xmake l -c 'import("core.project.project"); local t = project.target("redeschat"); print(t:pkgenvs().LD_LIBRARY_PATH or "")')"
if [[ -z "$runtime_paths" ]]; then
    printf 'Erro: o Xmake nao informou os caminhos das bibliotecas.\n' >&2
    exit 1
fi

qt_root=""
IFS=':' read -r -a search_paths <<< "$runtime_paths"
for library_dir in "${search_paths[@]}"; do
    if [[ -f "$library_dir/libQt6Core.so.6" && -d "${library_dir%/lib}/plugins" ]]; then
        qt_root="${library_dir%/lib}"
        break
    fi
done

if [[ -z "$qt_root" ]]; then
    printf 'Erro: instalacao do Qt 6 usada pelo Xmake nao foi localizada.\n' >&2
    exit 1
fi

rm -rf "$package_dir" "$archive" "$appimage" "$run_file" "$work_dir"
mkdir -p "$package_dir/bin" "$package_dir/lib" \
    "$package_dir/plugins/platforms" "$package_dir/plugins/sqldrivers" \
    "$work_dir/deb-root" "$dependency_cache/debs"
cp "$build_file" "$package_dir/bin/redeschat"

plugins=(
    "platforms/libqxcb.so"
    "platforms/libqoffscreen.so"
    "sqldrivers/libqsqlite.so"
)

dependency_queue=("$package_dir/bin/redeschat")
for plugin in "${plugins[@]}"; do
    source_file="$qt_root/plugins/$plugin"
    if [[ ! -f "$source_file" ]]; then
        printf 'Erro: plugin Qt obrigatorio nao encontrado: %s\n' "$source_file" >&2
        exit 1
    fi
    cp -L "$source_file" "$package_dir/plugins/$plugin"
    dependency_queue+=("$package_dir/plugins/$plugin")
done

# The prebuilt Qt XCB plugin expects these small runtime libraries. Downloading
# happens only while packaging; the generated artifacts never access the network.
apt_packages=(
    libxcb-cursor0 libxcb-icccm4 libxcb-image0 libxcb-keysyms1
    libxcb-render-util0 libxcb-util1 libxkbcommon-x11-0
)
if LD_LIBRARY_PATH="$runtime_paths" ldd "$qt_root/plugins/platforms/libqxcb.so" \
    | rg -q '=> not found'; then
    for apt_package in "${apt_packages[@]}"; do
        if ! compgen -G "$dependency_cache/debs/${apt_package}_*.deb" >/dev/null; then
            (
                cd "$dependency_cache/debs"
                apt-get -o APT::Sandbox::User=root download "$apt_package"
            )
        fi
    done

    for deb_file in "$dependency_cache"/debs/*.deb; do
        dpkg-deb -x "$deb_file" "$work_dir/deb-root"
    done
fi

shopt -s nullglob
for library_dir in "$work_dir/deb-root/lib/x86_64-linux-gnu" \
    "$work_dir/deb-root/usr/lib/x86_64-linux-gnu"; do
    for library_file in "$library_dir"/*.so.*; do
        cp -L "$library_file" "$package_dir/lib/$(basename -- "$library_file")"
    done
done

# Bundle the complete non-glibc dependency closure. Keeping glibc from the host
# avoids unsafe NSS and loader behavior while preserving broad Linux support.
export LD_LIBRARY_PATH="$package_dir/lib:$runtime_paths"
declare -A copied_libraries=()
core_libraries=(
    ld-linux-x86-64.so.2 libc.so.6 libdl.so.2 libgcc_s.so.1 libm.so.6
    libpthread.so.0 libresolv.so.2 librt.so.1
)
queue_index=0
while (( queue_index < ${#dependency_queue[@]} )); do
    current_file="${dependency_queue[$queue_index]}"
    ((queue_index += 1))

    while IFS= read -r dependency_line; do
        dependency_path=""
        if [[ "$dependency_line" =~ \=\>\ (/[^[:space:]]+) ]]; then
            dependency_path="${BASH_REMATCH[1]}"
        elif [[ "$dependency_line" =~ ^[[:space:]]*(/[^[:space:]]+) ]]; then
            dependency_path="${BASH_REMATCH[1]}"
        fi

        [[ -n "$dependency_path" ]] || continue

        library_name="$(basename -- "$dependency_path")"
        is_core=false
        for core_library in "${core_libraries[@]}"; do
            if [[ "$library_name" == "$core_library" ]]; then
                is_core=true
                break
            fi
        done
        [[ "$is_core" == false ]] || continue
        [[ -z "${copied_libraries[$library_name]+set}" ]] || continue

        destination="$package_dir/lib/$library_name"
        if [[ ! -f "$destination" ]]; then
            cp -L "$dependency_path" "$destination"
        fi
        copied_libraries[$library_name]=1
        dependency_queue+=("$destination")
    done < <(LD_LIBRARY_PATH="$package_dir/lib:$runtime_paths" ldd "$current_file")
done

declare -A unresolved_dependencies=()
for packaged_file in "$package_dir/bin/redeschat" \
    "$package_dir/plugins/platforms/libqxcb.so" \
    "$package_dir/plugins/sqldrivers/libqsqlite.so"; do
    while IFS= read -r dependency_line; do
        if [[ "$dependency_line" =~ ^[[:space:]]*([^[:space:]]+)[[:space:]]+\=\>[[:space:]]+not[[:space:]]+found ]]; then
            unresolved_dependencies["${BASH_REMATCH[1]}"]=1
        fi
    done < <(LD_LIBRARY_PATH="$package_dir/lib" ldd "$packaged_file")
done

if (( ${#unresolved_dependencies[@]} > 0 )); then
    printf 'Erro: dependencias nao resolvidas no pacote:\n' >&2
    for dependency_name in "${!unresolved_dependencies[@]}"; do
        printf '  %s\n' "$dependency_name" >&2
    done
    exit 1
fi

# Ubuntu 22.04 provides glibc 2.35. Refuse artifacts requiring anything newer
# so rebuilding on another development machine cannot silently reduce support.
glibc_too_new=false
shopt -s globstar
for packaged_file in "$package_dir/bin/redeschat" "$package_dir/lib"/*.so.* \
    "$package_dir/plugins"/**/*.so; do
    while IFS= read -r glibc_symbol; do
        glibc_minor="${glibc_symbol#GLIBC_2.}"
        if [[ "$glibc_minor" =~ ^[0-9]+$ ]] && (( glibc_minor > 35 )); then
            printf 'Erro: %s exige %s, superior ao limite GLIBC_2.35.\n' \
                "$packaged_file" "$glibc_symbol" >&2
            glibc_too_new=true
        fi
    done < <(objdump -T "$packaged_file" 2>/dev/null | rg -o 'GLIBC_2\.[0-9]+' | sort -Vu || true)
done
[[ "$glibc_too_new" == false ]] || exit 1

cat > "$package_dir/RedesChat" <<'LAUNCHER'
#!/usr/bin/env bash
set -euo pipefail

app_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$app_dir/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export QT_PLUGIN_PATH="$app_dir/plugins"
exec "$app_dir/bin/redeschat" "$@"
LAUNCHER

cat > "$package_dir/LEIA-ME.txt" <<'INSTRUCTIONS'
RedesChat para Linux x86_64

Este diretorio e o conteudo extraido do ZIP:
1. Abra um terminal neste diretorio.
2. Execute: ./RedesChat

O launcher e as pastas bin, lib e plugins devem permanecer juntos.
Nao e necessario instalar Qt, Xmake, bibliotecas XCB ou acessar a internet.
Este pacote ainda precisa ser validado na distribuicao Linux do laboratorio.
INSTRUCTIONS

chmod +x "$package_dir/RedesChat" "$package_dir/bin/redeschat"
cp "$project_dir/docs/teste-laboratorio.md" "$package_dir/TESTE-LABORATORIO.md"
cp "$package_dir/RedesChat" "$package_dir/AppRun"
cp "$project_dir/packaging/redeschat.svg" "$package_dir/redeschat.svg"
cat > "$package_dir/redeschat.desktop" <<'DESKTOP'
[Desktop Entry]
Type=Application
Name=RedesChat
Exec=RedesChat
Icon=redeschat
Categories=Network;Chat;
Terminal=false
DESKTOP
chmod +x "$package_dir/AppRun"

(
    cd "$dist_dir"
    zip -q -r "$archive" "$package_name"
)

tar -C "$dist_dir" -czf "$work_dir/payload.tar.gz" "$package_name"
cp "$project_dir/packaging/portable-run-header.sh" "$run_file"
cat "$work_dir/payload.tar.gz" >> "$run_file"
chmod +x "$run_file"

runtime_file="$dependency_cache/runtime-x86_64"
if [[ ! -x "$runtime_file" ]]; then
    curl --fail --location --output "$runtime_file" \
        "https://github.com/AppImage/type2-runtime/releases/download/continuous/runtime-x86_64"
    chmod +x "$runtime_file"
fi
mksquashfs "$package_dir" "$work_dir/redeschat.squashfs" \
    -root-owned -noappend -comp zstd -quiet
cat "$runtime_file" "$work_dir/redeschat.squashfs" > "$appimage"
chmod +x "$appimage"

printf 'Artefatos criados:\n  %s\n  %s\n  %s\n' \
    "$appimage" "$run_file" "$archive"
