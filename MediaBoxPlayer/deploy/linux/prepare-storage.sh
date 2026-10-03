#!/bin/sh
# Prepare system-wide MediaBox storage for the account running Manager and Player.
set -eu

if [ "$#" -eq 1 ] && [ "$1" = "--help" ]; then
    printf 'Usage: sudo %s USER\n' "$0"
    exit 0
fi
if [ "$#" -ne 1 ]; then
    printf 'Usage: sudo %s USER\n' "$0" >&2
    exit 1
fi
if [ "$(id -u)" -ne 0 ]; then
    printf '%s\n' 'Run this script as root (for example with sudo).' >&2
    exit 1
fi

storage_uid=$(id -u -- "$1")
storage_gid=$(id -g -- "$1")
if [ "$storage_uid" -eq 0 ]; then
    printf '%s\n' 'Choose the non-root account that runs the audio session and MediaBoxManager.' >&2
    exit 1
fi

# Do not follow links or change ownership outside these dedicated directories.
for directory in /etc/mediabox /etc/mediabox/mediaboxmanager \
    /etc/mediabox/mediaboxplayer; do
    if [ -L "$directory" ] || { [ -e "$directory" ] && [ ! -d "$directory" ]; }; then
        printf 'Expected a normal directory: %s\n' "$directory" >&2
        exit 1
    fi
done

# Group write on the common root lets this account create shared station files.
install -d -o root -g "$storage_gid" -m 0770 /etc/mediabox
install -d -o "$storage_uid" -g "$storage_gid" -m 0750 /etc/mediabox/mediaboxmanager
install -d -o "$storage_uid" -g "$storage_gid" -m 0700 /etc/mediabox/mediaboxplayer

printf 'MediaBox storage prepared for %s (uid %s, gid %s).\n' "$1" "$storage_uid" "$storage_gid"
printf '%s\n' 'Existing files are preserved; their ownership and permissions are unchanged.'
