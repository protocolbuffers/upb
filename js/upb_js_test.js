
/**
 * Returns a promise that waits until a predicate is true.
 *
 * @param {predicate!} The predicate we will test until it is true.
 * @return {Promise!} A promise that will yield when the predicate is true.
 */
function waitUntil(predicate) {
  return new Promise(function(resolve, reject) {
    function tryAgain() {
      if (predicate()) {
        resolve();
      } else {
        setTimeout(tryAgain, 5);
      }
    }
    tryAgain();
  });
}

/**
 * Returns true if Emscripten has fully loaded and initialized the WASM
 * module.
 *
 * @return {bool!} Whether Emscripten has initialized.
 */
function isWasmInitialized() {
  return typeof runtimeInitialized != 'undefined' && runtimeInitialized;
}

describe('arena test', function() {
  it('creates and destroys an arena', async function() {
    await waitUntil(isWasmInitialized);
    let arena = _upb_Arena_New();
    expect(arena).toBeDefined();
    _upb_Arena_Free(arena);
  });
});
