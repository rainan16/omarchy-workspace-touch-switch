#!/usr/bin/env bash
set -euo pipefail

PLUGIN_ID="rainan16.workspace-touch-switch"
PLUGIN_DIR=$(cd "$(dirname "$0")" && pwd)
SHELL_JSON="${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/shell.json"
USER_NAME=$(id -un)
tmp=""

fail() {
  echo "setup-two-finger: $*" >&2
  exit 1
}

cleanup() {
  [[ -n $tmp && -e $tmp ]] && rm -f "$tmp"
}

trap cleanup EXIT

[[ $(id -u) -eq 0 ]] && fail "run as your user, not root"
[[ -f $PLUGIN_DIR/manifest.json ]] || fail "manifest.json missing; run this from the plugin directory"
[[ -f $PLUGIN_DIR/touch-gesture-daemon.c ]] || fail "touch-gesture-daemon.c missing"

need() {
  command -v "$1" >/dev/null 2>&1
}

if ! need jq || ! need gcc || ! need make; then
  need omarchy || fail "need jq, gcc, and make (or omarchy pkg add)"
  echo "Installing jq gcc make"
  omarchy pkg add jq gcc make
fi

need jq && need gcc || fail "need jq and gcc"

manifest_id=$(jq -r '.id // empty' "$PLUGIN_DIR/manifest.json")
[[ $manifest_id == "$PLUGIN_ID" ]] || fail "unexpected plugin id: $manifest_id"

echo "Building touch-gesture-daemon in $PLUGIN_DIR"
tmp=$(mktemp "$PLUGIN_DIR/.touch-gesture-daemon.XXXXXX")
gcc -O2 -Wall -o "$tmp" "$PLUGIN_DIR/touch-gesture-daemon.c"
mv -f "$tmp" "$PLUGIN_DIR/touch-gesture-daemon"
tmp=""
chmod 755 "$PLUGIN_DIR/touch-gesture-daemon"

in_input() {
  if [[ $# -eq 0 ]]; then
    id -nG
  else
    id -nG "$1"
  fi | tr ' ' '\n' | grep -qx input
}

if ! in_input "$USER_NAME"; then
  echo "Adding $USER_NAME to the input group"
  sudo usermod -aG input "$USER_NAME"
fi

if ! in_input; then
  cat <<EOF
Log out and back in so the input group applies, then run:

  $PLUGIN_DIR/setup-two-finger.sh

EOF
  exit 0
fi

[[ -f $SHELL_JSON ]] || fail "missing $SHELL_JSON"

if need omarchy; then
  if omarchy plugin list --json | jq -e --arg id "$PLUGIN_ID" 'any(.[]; .id == $id)' >/dev/null; then
    if ! omarchy plugin list --json | jq -e --arg id "$PLUGIN_ID" 'any(.[]; .id == $id and .enabled == true)' >/dev/null; then
      echo "Enabling $PLUGIN_ID"
      omarchy plugin enable "$PLUGIN_ID"
    fi
  else
    fail "plugin is not installed; run: omarchy plugin add https://github.com/rainan16/omarchy-workspace-touch-switch.git --enable"
  fi
fi

echo "Setting twoFinger on $PLUGIN_ID in $SHELL_JSON"
cp -a "$SHELL_JSON" "$SHELL_JSON.bak.$(date +%s)"
tmp=$(mktemp)
jq --arg id "$PLUGIN_ID" '
  if type != "object" then error("shell.json is not an object") else . end
  | .plugins //= []
  | if (.plugins | type) != "array" then error("plugins is not an array") else . end
  | if any(.plugins[]?; .id == $id) then
      .plugins |= map(if .id == $id then . + {twoFinger: true} else . end)
    else
      .plugins += [{id: $id, twoFinger: true}]
    end
' "$SHELL_JSON" >"$tmp"
mv -f "$tmp" "$SHELL_JSON"
tmp=""

echo "Two-finger swipes are on. Edge strips are hidden; the daemon does not grab the device."
