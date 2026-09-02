import QtQuick
import Quickshell.Io
import Quickshell.Hyprland
import "GestureModel.js" as GestureModel

Item {
  id: root
  property var shell: null
  property var manifest: null
  property int restartCount: 0
  property bool daemonDenied: false
  property bool giveUp: false

  readonly property string pluginDir: manifest && manifest.__sourceDir
    ? String(manifest.__sourceDir) : ""
  readonly property var pluginSettings: GestureModel.pluginSettings(
    shell && shell.shellConfig, manifest && manifest.id)
  readonly property bool osdEnabled: GestureModel.osdEnabled(pluginSettings)
  readonly property bool overlayEnabled: GestureModel.overlayEnabled(pluginSettings)
  readonly property bool twoFingerEnabled: GestureModel.twoFingerEnabled(pluginSettings)

  function log(msg) {
    console.warn("[touch-gestures] " + msg)
  }

  function onGesture(line) {
    root.log("stdout: " + line)
    var event = GestureModel.parseLine(line)
    if (!event) {
      root.log("parse skipped")
      return
    }
    root.handleEvent(event)
  }

  function handleEvent(event) {
    if (!event || !event.dispatch) return
    if (osdTimer.running) {
      root.log("busy, skip " + event.dispatch)
      return
    }
    var request = GestureModel.hyprRequest(event.dispatch)
    root.log("dispatch " + request + " usingLua=" + Hyprland.usingLua)
    Hyprland.dispatch(request)
    if (root.overlayEnabled) root.showOverlay()
    else if (root.osdEnabled) osdTimer.start()
  }

  function showOverlay() {
    if (!root.overlayEnabled) {
      root.log("overlay skipped: disabled")
      return
    }
    if (!shell) {
      root.log("overlay skipped: no shell")
      return
    }
    var id = manifest && manifest.id
    if (!id) {
      root.log("overlay skipped: no manifest")
      return
    }
    var ws = Hyprland.focusedWorkspace
    var name = ws && (ws.name || String(ws.id)) || ""
    root.log("overlay workspace=" + name)
    shell.summon(id, JSON.stringify(GestureModel.overlayPayload(name)))
  }

  function showInputDenied() {
    root.log(GestureModel.inputGroupLog())
    if (toastProcess.running) return
    toastProcess.command = GestureModel.inputGroupNotifyCommand()
    toastProcess.running = true
  }

  function showOsd() {
    if (!root.osdEnabled) {
      root.log("osd skipped: disabled")
      return
    }
    if (!shell) {
      root.log("osd skipped: no shell")
      return
    }
    var ws = Hyprland.focusedWorkspace
    var name = ws && (ws.name || String(ws.id)) || ""
    root.log("osd workspace=" + name)
    shell.summon("omarchy.osd", JSON.stringify(GestureModel.osdPayload(name)))
  }

  onTwoFingerEnabledChanged: {
    if (!root.twoFingerEnabled) {
      daemon.running = false
      return
    }
    root.restartCount = 0
    root.daemonDenied = false
    root.giveUp = false
    if (root.pluginDir !== "" && !daemon.running)
      daemon.running = true
  }

  Process {
    id: daemon
    command: [root.pluginDir + "/touch-gesture-daemon"]
    running: root.pluginDir !== "" && root.twoFingerEnabled
    stdout: SplitParser {
      onRead: function(line) { root.onGesture(line) }
    }
    stderr: SplitParser {
      onRead: function(line) {
        root.log("daemon: " + line)
        if (GestureModel.isPermissionDenied(line))
          root.daemonDenied = true
      }
    }
    onStarted: root.log("daemon started " + root.pluginDir)
    onExited: function(exitCode) {
      root.log("daemon exit " + exitCode + " restarts=" + root.restartCount)
      if (!root.twoFingerEnabled)
        return
      if (root.giveUp)
        return
      if (root.daemonDenied) {
        root.giveUp = true
        root.restartCount = 5
        root.showInputDenied()
        return
      }
      if (root.restartCount >= 5) {
        root.log("daemon giving up")
        root.giveUp = true
        return
      }
      root.restartCount += 1
      restartTimer.start()
    }
  }

  Process {
    id: toastProcess
  }

  Timer {
    id: osdTimer
    interval: 80
    onTriggered: root.showOsd()
  }

  Timer {
    id: restartTimer
    interval: 1000
    onTriggered: {
      if (root.twoFingerEnabled && !root.giveUp && !daemon.running)
        daemon.running = true
    }
  }

  Component.onCompleted: {
    root.log("loaded pluginDir=" + root.pluginDir)
  }
}
