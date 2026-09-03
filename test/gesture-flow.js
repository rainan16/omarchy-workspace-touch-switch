var test = require("node:test")
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

test("daemon swipe-right shows OSD", function () {
  assert.deepStrictEqual(handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1"), {
    request: 'hl.dsp.focus({ workspace = "e-1" })',
    osd: { icon: "touch", message: "workspace 1", duration: "800" }
  })
})

test("daemon swipe-left shows OSD", function () {
  assert.deepStrictEqual(handleDaemonLine('{"gesture":"swipe-left","direction":"prev"}', "2"), {
    request: 'hl.dsp.focus({ workspace = "e+1" })',
    osd: { icon: "touch", message: "workspace 2", duration: "800" }
  })
})

test("overlay skips OSD", function () {
  assert.deepStrictEqual(handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", { overlay: true }), {
    request: 'hl.dsp.focus({ workspace = "e-1" })',
    overlay: { workspace: "1", duration: "1500" }
  })
})

test("osd false is silent", function () {
  assert.deepStrictEqual(handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", {
    overlay: false,
    osd: false
  }), {
    request: 'hl.dsp.focus({ workspace = "e-1" })'
  })
})

test("twoFinger keeps OSD", function () {
  assert.deepStrictEqual(handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", { twoFinger: true }), {
    request: 'hl.dsp.focus({ workspace = "e-1" })',
    osd: { icon: "touch", message: "workspace 1", duration: "800" }
  })
})

test("twoFinger overlay skips OSD", function () {
  assert.deepStrictEqual(handleDaemonLine('{"gesture":"swipe-right","direction":"next"}', "1", {
    twoFinger: true,
    overlay: true
  }), {
    request: 'hl.dsp.focus({ workspace = "e-1" })',
    overlay: { workspace: "1", duration: "1500" }
  })
})

test("edge swipe-right shows OSD", function () {
  assert.deepStrictEqual(
    handleEvent(GestureModel.classifyEdgeSwipe("left", 20, 200, 500, 500, 1000, 1000), "1"),
    {
      request: 'hl.dsp.focus({ workspace = "e-1" })',
      osd: { icon: "touch", message: "workspace 1", duration: "800" }
    }
  )
})

test("edge swipe-left shows OSD", function () {
  assert.deepStrictEqual(
    handleEvent(GestureModel.classifyEdgeSwipe("right", 980, 800, 500, 500, 1000, 1000), "2"),
    {
      request: 'hl.dsp.focus({ workspace = "e+1" })',
      osd: { icon: "touch", message: "workspace 2", duration: "800" }
    }
  )
})

test("short edge swipe is ignored", function () {
  assert.strictEqual(handleEvent(GestureModel.classifyEdgeSwipe("left", 20, 50, 500, 500, 1000, 1000), "1"), null)
})

test("invalid daemon line is ignored", function () {
  assert.strictEqual(handleDaemonLine("not-json", "1"), null)
  assert.strictEqual(handleDaemonLine("", "1"), null)
})
