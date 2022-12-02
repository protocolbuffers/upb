
// This disables inlining and forces all public functions to be exported.
#define UPB_BUILD_API

#include "upb/mem/arena.h"

// This is unused when we compile for WASM, but Builder doesn't like it
// if this file cannot compile as a regular cc_binary().
int main() {}
