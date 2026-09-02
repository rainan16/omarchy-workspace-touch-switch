var assert = require("assert")
var GestureModel = require("../GestureModel.js")

function handleEvent(event, workspaceName, settings) {
  if (!event) return null
  var out = { request: GestureModel.hyprRequest(event.dispatch) }
  if (GestureModel.overlayEnabled(settings))
    out.overlay = GestureModel.overlayPayload(workspaceName)
  else if (GestureModel.osdAfterSwipe(settings))
    out.osd = GestureModel.osdPayload(workspaceName)
  return out
}

function handleDaemonLine(line, workspaceName, settings) {
  return handleEvent(GestureModel.parseLine(line), workspaceName, settings)
}

var right = handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1")
assert.deepStrictEqual(right, {
  request: 'hl.dsp.focus({ workspace = "e-1" })',
  osd: { icon: "touch", message: "workspace 1", duration: "800" }
})

var left = handleDaemonLine('{"gesture":"swipe-left","direction":"prev"}', "2")
assert.deepStrictEqual(left, {
  request: 'hl.dsp.focus({ workspace = "e+1" })',
  osd: { icon: "touch", message: "workspace 2", duration: "800" }
})

var overlayOn = handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", { overlay: true })
assert.deepStrictEqual(overlayOn, {
  request: 'hl.dsp.focus({ workspace = "e-1" })',
  overlay: { workspace: "1", duration: "1500" }
})

var silent = handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", {
  overlay: false,
  osd: false
})
assert.deepStrictEqual(silent, {
  request: 'hl.dsp.focus({ workspace = "e-1" })'
})

var twoFinger = handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", { twoFinger: true })
assert.deepStrictEqual(twoFinger, {
  request: 'hl.dsp.focus({ workspace = "e-1" })',
  osd: { icon: "touch", message: "workspace 1", duration: "800" }
})

var twoFingerOverlay = handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", {
  twoFinger: true,
  overlay: true
})
assert.deepStrictEqual(twoFingerOverlay, {
  request: 'hl.dsp.focus({ workspace = "e-1" })',
  overlay: { workspace: "1", duration: "1500" }
})

assert.deepStrictEqual(
  handleEvent(GestureModel.classifyEdgeSwipe("left", 20, 200, 500, 500, 1000, 1000), "1"),
  {
    request: 'hl.dsp.focus({ workspace = "e-1" })',
    osd: { icon: "touch", message: "workspace 1", duration: "800" }
  }
)
assert.deepStrictEqual(
  handleEvent(GestureModel.classifyEdgeSwipe("right", 980, 800, 500, 500, 1000, 1000), "2"),
  {
    request: 'hl.dsp.focus({ workspace = "e+1" })',
    osd: { icon: "touch", message: "workspace 2", duration: "800" }
  }
)
assert.strictEqual(handleEvent(GestureModel.classifyEdgeSwipe("left", 20, 50, 500, 500, 1000, 1000), "1"), null)

assert.strictEqual(handleDaemonLine("not-json", "1"), null)
assert.strictEqual(handleDaemonLine("", "1"), null)
