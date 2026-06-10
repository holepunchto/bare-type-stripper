#include <assert.h>
#include <bare.h>
#include <js.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "lib/strip.h"

static js_value_t *
bare_type_stripper_lex(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 1;
  js_value_t *argv[1];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  assert(argc == 1);

  utf8_t *input;
  size_t len;
  err = js_get_typedarray_info(env, argv[0], NULL, (void **) &input, (size_t *) &len, NULL, NULL);
  assert(err == 0);

  bare_type_stripper_t ctx;
  err = bare_type_stripper__lex(&ctx, input, len);

  if (err < 0) {
    free(ctx.ranges);

    err = js_throw_error(env, NULL, "Out of memory");
    assert(err == 0);

    return NULL;
  }

  // Hand the ranges to JS as a single Uint32Array of (start, end, flags)
  // triples.
  js_value_t *arraybuffer;
  uint32_t *data;
  err = js_create_arraybuffer(env, ctx.len * 3 * sizeof(uint32_t), (void **) &data, &arraybuffer);
  assert(err == 0);

  if (ctx.len) memcpy(data, ctx.ranges, ctx.len * 3 * sizeof(uint32_t));

  free(ctx.ranges);

  js_value_t *ranges;
  err = js_create_typedarray(env, js_uint32array, ctx.len * 3, arraybuffer, 0, &ranges);
  assert(err == 0);

  return ranges;
}

static js_value_t *
bare_type_stripper_exports(js_env_t *env, js_value_t *exports) {
  int err;

#define V(name, fn) \
  { \
    js_value_t *val; \
    err = js_create_function(env, name, -1, fn, NULL, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("lex", bare_type_stripper_lex)
#undef V

#define V(name, n) \
  { \
    js_value_t *val; \
    err = js_create_uint32(env, n, &val); \
    assert(err == 0); \
    err = js_set_named_property(env, exports, name, val); \
    assert(err == 0); \
  }

  V("SEMI", bare_type_stripper_semi)
  V("PAREN", bare_type_stripper_paren)
  V("ERROR", bare_type_stripper_error)
#undef V

  return exports;
}

BARE_MODULE(bare_type_stripper, bare_type_stripper_exports)
