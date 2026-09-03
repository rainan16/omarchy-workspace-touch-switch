# AGENTS.md

Omarchy `service` + `overlay` plugin. Touch swipes switch Hyprland workspaces and show live previews.

Plugin id: `rainan16.workspace-touch-switch`

Gestures (`GestureModel.js` dispatch, same object from strips or daemon JSON):

- one finger, left edge, swipe right → `swipe-right` → `e-1`
- one finger, right edge, swipe left → `swipe-left` → `e+1`
- two fingers, horizontal swipe anywhere → same JSON by direction (right → `e-1`, left → `e+1`)

Default: layer-shell edge strips in `Overlay.qml` (`keepLoaded`). No daemon. No `input` group. `"twoFinger": true` hides strips, starts `touch-gesture-daemon`, and does both one-finger edges and two-finger anywhere. Needs `input`, then re-login. Never run strips and the daemon together.

Overlay: off unless `plugins[]` entry has `"overlay": true`. Then `shell.summon(manifest.id, JSON.stringify(GestureModel.overlayPayload(name)))`. `Overlay.qml` is a separate kind; no UI in `Service.qml`. Live previews via `ScreencopyView` on each workspace toplevel's `wayland` handle. Pass `GestureModel.layoutSize(width, height, scale)` into `windowRect`, not raw IPC pixel size. Edge strips also live here (`visible: !twoFingerEnabled`).

OSD: on unless `"osd": false`, and skipped while overlay is on. Then `shell.summon("omarchy.osd", JSON.stringify(GestureModel.osdPayload(name)))`. Name from `Hyprland.focusedWorkspace`. Icon `touch` is OSD glyph `󰝁`. Read the entry via `GestureModel.pluginSettings(shell.shellConfig, manifest.id)`.

`twoFinger`: off unless `"twoFinger": true`. `GestureModel.twoFingerEnabled` / `daemonShouldRun`. Process `running: root.pluginDir !== "" && root.twoFingerEnabled`. Overlay/strips call `service.handleEvent(event)`.

Plan history: `PLAN.md`, `PLAN-edge-split.md`. Samples: `/usr/share/omarchy/shell/plugins`. Docs: https://plugins.omarchy.org/develop.html

## Setup

```bash
make
node --test test/gesture-model.js test/gesture-flow.js test/setup-two-finger.js
omarchy plugin validate "$PWD"
/usr/lib/qt6/bin/qmllint -I "${OMARCHY_PATH:-/usr/share/omarchy}/shell" Service.qml Overlay.qml
```

`qmllint` is not on `PATH`. `OMARCHY_PATH` is `/usr/share/omarchy`. `input` group is required only for `"twoFinger": true`, then re-login (`groups` must list `input`).

Install (after `make`):

```bash
mkdir -p ~/.config/omarchy/plugins/rainan16.workspace-touch-switch
cp manifest.json Service.qml Overlay.qml GestureModel.js touch-gesture-daemon Makefile \
  ~/.config/omarchy/plugins/rainan16.workspace-touch-switch/
omarchy plugin enable rainan16.workspace-touch-switch
omarchy restart shell
```

Replace a running daemon with `mv`, not `cp` onto the same path (`ETXTBSY`). Then `omarchy restart shell`.

```bash
cp touch-gesture-daemon ~/.config/omarchy/plugins/rainan16.workspace-touch-switch/touch-gesture-daemon.new
mv -f ~/.config/omarchy/plugins/rainan16.workspace-touch-switch/touch-gesture-daemon.new \
  ~/.config/omarchy/plugins/rainan16.workspace-touch-switch/touch-gesture-daemon
omarchy restart shell
```

## Testing

On every important change: update `README.md` (usage, install, configure, mapping) and update tests. Then run all of them — `make`, `node --test test/gesture-model.js test/gesture-flow.js test/setup-two-finger.js`, `omarchy plugin validate "$PWD"`, and qmllint as above. Do not skip tests because a change “looks small”.

- C: `make` with `-Wall` and no errors.
- JS: `node --test test/gesture-model.js test/gesture-flow.js test/setup-two-finger.js`.
- Evdev and live Hyprland are not in node tests; swipe on device after `omarchy restart shell`.
- QML/manifest: `omarchy plugin validate "$PWD"` then qmllint as above.
- Daemon: only when `"twoFinger": true`. `./touch-gesture-daemon`, swipe, one JSON line then flush.
- Default (no twoFinger, no `input`): one-finger edge swipe switches workspace; no daemon in `journalctl -t touch-gesture-daemon`.
- `"twoFinger": true` + `input`: two-finger anywhere and edges (daemon); strips gone.
- `"twoFinger": true` without `input`: daemon `cannot open`, no restart storm, no edge strips, toast `Touch error: twoFinger mode without 'input' group`, log says disable twoFinger or add `input` and re-login.
- Green validate is not “gestures work”. That only checks the plugin contract.
- After QML/JS/C install, `omarchy-shell shell rescanPlugins` is not enough. Restart the shell or you will debug stale code.
- `qmllint` `onExited` ExitStatus warnings match first-party services; exit 0 is accept.

Logs:

```bash
journalctl -t touch-gesture-daemon -f
journalctl --user -f | grep touch-gestures
```

Daemon uses syslog (`LOG_USER`). QML uses `console.warn("[touch-gestures] …")` (WARN). `console.log` is DEBUG and easy to miss.

## Code style

Copy first-party services (`idle`, `battery`, `media`), not bar widgets.

- `Service.qml` root is `Item {}`. Inject `property var shell: null` and `property var manifest: null`.
- `import Quickshell.Io`. Static `Process` in the tree. Daemon stdout: `SplitParser { onRead: … }`.
- Binary path: `manifest.__sourceDir + "/touch-gesture-daemon"`. Not `Qt.resolvedUrl`.
- `manifest` is injected after `Component.onCompleted`. `pluginDir` is empty there. Bind `running: root.pluginDir !== "" && root.twoFingerEnabled`.
- Parse daemon JSON and classify edge-strip swipes in `GestureModel.js`. Keep QML as wiring. Overlay has `property var service`; strips call `service.handleEvent`.
- Workspace switch: `Hyprland.dispatch("hl.dsp.focus({ workspace = \"" + event.dispatch + "\" })")`.
- No comments unless asked.

C daemon:

- Scan `/sys/class/input/event*` via sysfs (name, ev/abs bits, `properties`). Do not require opening `/dev/input` to discover.
- Prefer case-insensitive name match: `ntrg`, `n-trig`, `atml`, `elan`, `goodix`, `wacom`, `surface`. Among matches, prefer `INPUT_PROP_DIRECT` so “Microsoft Surface Keyboard Touchpad” loses to ELAN/N-trig.
- Else first multi-touch device with `INPUT_PROP_DIRECT`.
- Abs-range ratios (`EDGE_RATIO`, `SWIPE_RATIO`), not hardcoded 1920.
- Include `<sys/ioctl.h>`. Never `EVIOCGRAB`.
- One finger: slot protocol B, edge classify on `SYN_REPORT` after down and after `ABS_MT_POSITION_X`.
- Two fingers: exactly two contacts, centroid, anywhere; emit when count drops below 2. Three or more cancels. Require `|dx| >= swipe_min` and horizontal (`|dx|/span_x > |dy|/span_y`).
- JSON + `fflush` on stdout only. Logs go to syslog.

## Gotchas

- This Hyprland is Lua (0.56). `hyprctl dispatch workspace e+1` fails with `')' expected near 'e'`. OSD will still show the current workspace. First-party bar uses `hl.dsp.focus({ workspace = "…" })`. `Hyprland.dispatch` talks IPC with that string. Do not spawn `hyprctl` for this.
- QML `import "GestureModel.js"` is cached. File copy + rescan often leaves the old mapping. `omarchy restart shell` after Service.qml or GestureModel.js changes.
- `cp` onto a running `touch-gesture-daemon` fails with `Text file busy`. `mv` over the name, then restart the shell so it execs the new inode.
- Discovery `open()` on `/dev/input/event*` without `input` group looks like “no touchscreen”. Sysfs scan still finds ELAN9038; the real error is `Permission denied` on open. Default path does not open `/dev/input`.
- `TOUCHSCREEN_DEVICE` overrides discovery. Env `EDGE_RATIO` (default 0.04) and `SWIPE_RATIO` (default 0.08) override daemon thresholds. Strips use the same ratios in `GestureModel.edgeWidth` / `swipeMin`.
- Do not grab the device. Two readers (manual `./touch-gesture-daemon` and the plugin in two-finger mode) both see events; kill the manual one before testing the desktop.
- End-user README Install is only `omarchy plugin add … --enable`. Official add clones, validates, rescans, and enables. Do not put `make`, gcc, `omarchy restart shell`, or the `input` group there. Default path is QML strips. `make` (daemon is gitignored) and `input` then re-login belong under `"twoFinger": true` only (`setup-two-finger.sh` or the README manual steps). Restart after QML/JS is a local-dev cache issue, not first install. `plugin add` must not run that script.
- Strips consume the edge band (~4% of width). The daemon path does not. Without `input` and two-finger on: daemon logs `cannot open`, one-finger also gone until twoFinger is turned off.
- `Hyprland.focusedMonitor.width/height` are physical pixels. Client `at`/`size` and monitor `x`/`y` are layout. `windowRect` needs `width/scale` and `height/scale`. Scale 1 hides half-size previews stuck in the card's top-left.

## Security

- Plugins run unsandboxed inside `omarchy-shell` with the user’s permissions.
- Never `EVIOCGRAB`. Never commit secrets. Do not add install hooks or a second Quickshell process. Plugin runtime must not sudo. `setup-two-finger.sh` is user-invoked only and may sudo solely for `usermod -aG input`. `uninstall-two-finger.sh` is user-invoked, does not sudo, does not drop `input`, and does not run `omarchy plugin remove`.

## Boundaries

Never:

- Clone `omarchy.clock` or any `bar-widget` as a starting point.
- Put UI (`Rectangle`, `PanelWindow`, `NotificationWindow`) in `Service.qml`. Overlay UI lives in `Overlay.qml`.
- Use `omarchy-notification-send` for workspace switches. This is overlay/OSD.
- Use `Qt.createQmlObject` for `Process`.
- Ship only a prebuilt binary. The binary is gitignored. `make` is a twoFinger and local-dev step, not default end-user install.
