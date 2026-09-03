var test = require("node:test")
var assert = require("assert")
var fs = require("fs")

test("releaserc releases only main, bumps manifest, and does not publish npm", function () {
  var cfg = JSON.parse(fs.readFileSync(".releaserc.json", "utf8"))
  var plugins = JSON.stringify(cfg.plugins)
  assert.deepStrictEqual(cfg.branches, ["main"])
  assert.strictEqual(cfg.tagFormat, "v${version}")
  assert.ok(plugins.indexOf("@semantic-release/npm") === -1)
  assert.ok(plugins.indexOf("scripts/bump-manifest.js") !== -1)
  assert.ok(plugins.indexOf("manifest.json") !== -1)
})
