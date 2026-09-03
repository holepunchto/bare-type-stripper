#include <assert.h>
#include <bare.h>
#include <js.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>

#include "lib/strip.h"

static int
bare_type_stripper__get_int64(js_env_t *env, js_value_t *value, int64_t *result) {
  int err;

  bool is_number;
  err = js_is_number(env, value, &is_number);
  assert(err == 0);

  if (!is_number) return -1;

  err = js_get_value_int64(env, value, result);
  assert(err == 0);

  return 0;
}

static js_value_t *
bare_type_stripper_lex(js_env_t *env, js_callback_info_t *info) {
  int err;

  size_t argc = 3;
  js_value_t *argv[3];

  err = js_get_callback_info(env, info, &argc, argv, NULL, NULL);
  assert(err == 0);

  bool is_arraybuffer;
  err = js_is_arraybuffer(env, argv[0], &is_arraybuffer);
  assert(err == 0);

  bool is_sharedarraybuffer;
  err = js_is_sharedarraybuffer(env, argv[0], &is_sharedarraybuffer);
  assert(err == 0);

  if (!is_arraybuffer && !is_sharedarraybuffer) {
    err = js_throw_type_error(env, NULL, "Input must be a buffer");
    assert(err == 0);

    return NULL;
  }

  void *data;
  size_t byte_len;

  if (is_sharedarraybuffer) {
    err = js_get_sharedarraybuffer_info(env, argv[0], &data, &byte_len);
  } else {
    err = js_get_arraybuffer_info(env, argv[0], &data, &byte_len);
  }

  assert(err == 0);

  int64_t offset;
  if (bare_type_stripper__get_int64(env, argv[1], &offset) < 0) {
    err = js_throw_type_error(env, NULL, "Offset must be a number");
    assert(err == 0);

    return NULL;
  }

  int64_t len;
  if (bare_type_stripper__get_int64(env, argv[2], &len) < 0) {
    err = js_throw_type_error(env, NULL, "Length must be a number");
    assert(err == 0);

    return NULL;
  }

  if (offset < 0 || len < 0 || (uint64_t) offset + (uint64_t) len > byte_len) {
    err = js_throw_range_error(env, NULL, "Offset and length are out of bounds");
    assert(err == 0);

    return NULL;
  }

  if ((uint64_t) len > UINT32_MAX) {
    err = js_throw_range_error(env, NULL, "Length is out of bounds");
    assert(err == 0);

    return NULL;
  }

  utf8_t *input = (utf8_t *) data + offset;

  // Another thread may write shared memory while we read it, and the ranges we
  // record must describe one input. Lex a private copy instead.
  utf8_t *copy = NULL;

  if (is_sharedarraybuffer && len > 0) {
    copy = malloc((size_t) len);

    if (copy == NULL) {
      err = js_throw_error(env, NULL, "Out of memory");
      assert(err == 0);

      return NULL;
    }

    memcpy(copy, input, (size_t) len);

    input = copy;
  }

  bare_type_stripper_t ctx;
  err = bare_type_stripper__lex(&ctx, input, (size_t) len);

  free(copy);

  if (err < 0) {
    free(ctx.ranges);

    err = js_throw_error(env, NULL, "Out of memory");
    assert(err == 0);

    return NULL;
  }

  // Hand the ranges to JS as a single Uint32Array of (start, end, flags)
  // triples.
  size_t size = (size_t) ctx.len * 3;

  js_value_t *arraybuffer;
  uint32_t *data_out;
  err = js_create_arraybuffer(env, size * sizeof(uint32_t), (void **) &data_out, &arraybuffer);
  assert(err == 0);

  if (ctx.len) memcpy(data_out, ctx.ranges, size * sizeof(uint32_t));

  free(ctx.ranges);

  js_value_t *ranges;
  err = js_create_typedarray(env, js_uint32array, size, arraybuffer, 0, &ranges);
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
