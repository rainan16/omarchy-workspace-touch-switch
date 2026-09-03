var test = require("node:test")
var assert = require("assert")
var fs = require("fs")
var path = require("path")

var scriptPath = "setup-two-finger.sh"
var readmePath = "README.md"

test("setup-two-finger.sh is a user-run bash setup, not an install hook", function () {
  var sh = fs.readFileSync(scriptPath, "utf8")
  var st = fs.statSync(scriptPath)
  assert.ok(st.mode & 0o111, "script must be executable")
  assert.ok(sh.indexOf("#!/usr/bin/env bash") === 0)
  assert.ok(sh.indexOf("set -euo pipefail") !== -1)
  assert.ok(sh.indexOf("usermod -aG input") !== -1)
  assert.ok(sh.indexOf("touch-gesture-daemon.c") !== -1)
  assert.ok(sh.indexOf("twoFinger") !== -1)
  assert.ok(sh.indexOf("EVIOCGRAB") === -1)
  assert.ok(sh.indexOf("run as your user, not root") !== -1)
})

test("uninstall-two-finger.sh turns off twoFinger without sudo or dropping input", function () {
  var sh = fs.readFileSync("uninstall-two-finger.sh", "utf8")
  var st = fs.statSync("uninstall-two-finger.sh")
  assert.ok(st.mode & 0o111, "script must be executable")
  assert.ok(sh.indexOf("#!/usr/bin/env bash") === 0)
  assert.ok(sh.indexOf("set -euo pipefail") !== -1)
  assert.ok(sh.indexOf("del(.twoFinger)") !== -1)
  assert.ok(sh.indexOf("usermod") === -1)
  assert.ok(sh.indexOf("sudo") === -1)
  assert.ok(sh.indexOf("gpasswd") === -1)
  assert.ok(sh.indexOf("omarchy plugin remove") !== -1)
  assert.ok(sh.indexOf("EVIOCGRAB") === -1)
  assert.ok(sh.indexOf("run as your user, not root") !== -1)
})

test("README two-finger section lists the script and manual steps", function () {
  var md = fs.readFileSync(readmePath, "utf8")
  assert.ok(md.indexOf("## Two-finger swipes") !== -1)
  assert.ok(md.indexOf("### Activate") !== -1)
  assert.ok(md.indexOf("### Deactivate") !== -1)
  assert.ok(md.indexOf("### Manual steps") !== -1)
  assert.ok(md.indexOf("setup-two-finger.sh") !== -1)
  assert.ok(md.indexOf("uninstall-two-finger.sh") !== -1)
  assert.ok(md.indexOf("sudo usermod -aG input") !== -1)
  assert.ok(md.indexOf("omarchy plugin add") !== -1)
  assert.ok(md.indexOf("setup-two-finger.sh") > md.indexOf("## Install"))
  assert.ok(md.indexOf("uninstall-two-finger.sh") > md.indexOf("### Deactivate"))
})

test("default Install does not require make or input", function () {
  var md = fs.readFileSync(readmePath, "utf8")
  var install = md.slice(md.indexOf("## Install"), md.indexOf("## Two-finger swipes"))
  assert.ok(install.indexOf("omarchy plugin add") !== -1)
  assert.ok(install.indexOf("make") === -1)
  assert.ok(install.indexOf("input") === -1)
  assert.ok(path.basename(scriptPath) === "setup-two-finger.sh")
})
