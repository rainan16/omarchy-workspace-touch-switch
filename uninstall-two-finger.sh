#!/usr/bin/env bash
set -euo pipefail

PLUGIN_ID="rainan16.workspace-touch-switch"
PLUGIN_DIR=$(cd "$(dirname "$0")" && pwd)
SHELL_JSON="${XDG_CONFIG_HOME:-$HOME/.config}/omarchy/shell.json"
tmp=""

fail() {
  echo "uninstall-two-finger: $*" >&2
  exit 1
}

cleanup() {
  [[ -n $tmp && -e $tmp ]] && rm -f "$tmp"
}

trap cleanup EXIT

[[ $(id -u) -eq 0 ]] && fail "run as your user, not root"
[[ -f $PLUGIN_DIR/manifest.json ]] || fail "manifest.json missing; run this from the plugin directory"

need() {
  command -v "$1" >/dev/null 2>&1
}

if ! need jq; then
  need omarchy || fail "need jq (or omarchy pkg add jq)"
  echo "Installing jq"
  omarchy pkg add jq
fi
need jq || fail "need jq"

manifest_id=$(jq -r '.id // empty' "$PLUGIN_DIR/manifest.json")
[[ $manifest_id == "$PLUGIN_ID" ]] || fail "unexpected plugin id: $manifest_id"

[[ -f $SHELL_JSON ]] || fail "missing $SHELL_JSON"

echo "Clearing twoFinger on $PLUGIN_ID in $SHELL_JSON"
cp -a "$SHELL_JSON" "$SHELL_JSON.bak.$(date +%s)"
tmp=$(mktemp)
jq --arg id "$PLUGIN_ID" '
  if type != "object" then error("shell.json is not an object") else . end
  | .plugins //= []
  | if (.plugins | type) != "array" then error("plugins is not an array") else . end
  | .plugins |= map(if .id == $id then del(.twoFinger) else . end)
' "$SHELL_JSON" >"$tmp"
mv -f "$tmp" "$SHELL_JSON"
tmp=""

rm -f "$PLUGIN_DIR/touch-gesture-daemon"

cat <<EOF
Two-finger swipes are off. Edge strips are back. The input group was left in place.

Remove the plugin with:

  omarchy plugin remove $PLUGIN_ID

EOF
