#!/bin/sh
set -eu

archive_line=$(awk '/^__REDESCHAT_ARCHIVE__$/ { print NR + 1; exit }' "$0")
if [ -z "$archive_line" ]; then
    printf '%s\n' 'Erro: conteudo interno do RedesChat nao encontrado.' >&2
    exit 1
fi

temp_dir=$(mktemp -d "${TMPDIR:-/tmp}/redeschat.XXXXXX")
cleanup() {
    rm -rf "$temp_dir"
}
trap cleanup EXIT HUP INT TERM

tail -n "+$archive_line" "$0" | tar -xzf - -C "$temp_dir"
"$temp_dir/RedesChat-linux-x86_64/RedesChat" "$@"
exit_code=$?
exit "$exit_code"

__REDESCHAT_ARCHIVE__
