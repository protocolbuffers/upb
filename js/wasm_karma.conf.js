/**
 * Config for Karma test runner to serve Maldoca WebAssembly binaries.
 *
 * @param {!Object} config
 */
module.exports = ((config) => {
  const SERVE_ARGS =
      {included: false, nocache: false, served: true, watched: false};
  const serve = (pattern) => {
    config.files.push({pattern: pattern, ...SERVE_ARGS});
  };

  serve('third_party/upb/js/upb_js_wasm/upb_js_c.js');
  serve('third_party/upb/js/upb_js_wasm/upb_js_c.wasm');
});
