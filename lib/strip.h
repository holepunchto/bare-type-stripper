#ifndef BARE_TYPE_STRIPPER_H
#define BARE_TYPE_STRIPPER_H

#include <assert.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <utf.h>

// Range flag bits. When set on a stripped range, they tell the JS apply
// layer to replace some bytes with something other than a space.
enum {
  bare_type_stripper_semi = 0x1,  // First byte of range becomes ';' instead of space
  bare_type_stripper_paren = 0x2, // First byte of range becomes ')' instead of space
  bare_type_stripper_error = 0x4  // Range marks non-erasable TS syntax that can't be stripped to valid JS
};

#define BARE_TYPE_STRIPPER__PAREN_CACHE_SIZE 512 // Power of two
#define BARE_TYPE_STRIPPER__PAREN_STACK_SIZE 512

// Lexer context. Carries the input, the accumulated strip ranges, and a
// small cache of paren match positions used by the arrow-function lookahead.
typedef struct {
  // Input being lexed.
  const utf8_t *s;
  size_t n;

  // Strip ranges as (start, end, flags) triples, heap-allocated and grown
  // geometrically. The caller owns the buffer and must free() it.
  uint32_t *ranges;
  uint32_t len; // In triples
  uint32_t cap; // In triples

  // Direct-mapped cache of paren match positions: paren_open[slot] is the
  // position of a '(' and paren_close[slot] the index just past its matching
  // ')'. A zero close marks an empty slot - a '(' at position 0 closes at
  // position 2 or later, so 0 can't be a valid close.
  size_t paren_open[BARE_TYPE_STRIPPER__PAREN_CACHE_SIZE];
  size_t paren_close[BARE_TYPE_STRIPPER__PAREN_CACHE_SIZE];
} bare_type_stripper_t;

// Previous-token classification, used to decide how to interpret the next
// character. Together with a flag for the previous '.' / '?.' (property
// access), this is enough to distinguish e.g. division from regex, generic
// arguments from less-than, postfix '!' from prefix '!', and TS contextual
// keywords from ordinary identifiers.
enum {
  bare_type_stripper__prev_none = 0,  // Start of input or block - statement start
  bare_type_stripper__prev_stmt = 1,  // After ';' or end of statement - statement start
  bare_type_stripper__prev_op = 2,    // After an operator, comma, opening bracket - expression start
  bare_type_stripper__prev_expr = 3,  // After an identifier, literal, ']' - expression end
  bare_type_stripper__prev_paren = 4, // After ')' - expression end, but a following '{' is a block
  bare_type_stripper__prev_arrow = 5, // After '=>' - expression start, but a following '{' is a function body
  bare_type_stripper__prev_async = 6  // After the identifier 'async' - expression end, but a following '(' may start arrow parameters
};

// True when `prev` marks the end of an expression - i.e. the next token
// continues or follows an expression rather than starting one.
static inline bool
bare_type_stripper__prev_is_expr(int prev) {
  return prev == bare_type_stripper__prev_expr || prev == bare_type_stripper__prev_paren || prev == bare_type_stripper__prev_async;
}

static inline bool
bare_type_stripper__is_line_terminator(uint8_t c) {
  return c == 0xa || c == 0xd;
}

static inline bool
bare_type_stripper__is_whitespace(uint8_t c) {
  // Byte-oriented: Multi-byte Unicode whitespace (NBSP, U+2028/2029) is not
  // recognized and lexes as identifier bytes instead, which is harmless for
  // stripping purposes and vanishingly rare in real source.
  return c == ' ' || c == '\t' || c == 0xb || c == 0xc || bare_type_stripper__is_line_terminator(c);
}

static inline bool
bare_type_stripper__is_id_start(uint8_t c) {
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$' || c >= 0x80;
}

static inline bool
bare_type_stripper__is_id(uint8_t c) {
  return bare_type_stripper__is_id_start(c) || (c >= '0' && c <= '9');
}

// Current character, unchecked
#define u(offset) ((uint8_t) s[i + offset])

#define lt  bare_type_stripper__is_line_terminator
#define ws  bare_type_stripper__is_whitespace
#define ids bare_type_stripper__is_id_start
#define id  bare_type_stripper__is_id

static inline int
bare_type_stripper__add_range_flags(bare_type_stripper_t *ctx, size_t start, size_t end, uint32_t flags) {
  assert(end > start);

  if (ctx->len == ctx->cap) {
    uint32_t cap = ctx->cap == 0 ? 64 : ctx->cap * 2;

    uint32_t *ranges = realloc(ctx->ranges, cap * 3 * sizeof(uint32_t));
    if (ranges == NULL) return -1;

    ctx->ranges = ranges;
    ctx->cap = cap;
  }

  uint32_t *entry = &ctx->ranges[ctx->len * 3];

  entry[0] = (uint32_t) start;
  entry[1] = (uint32_t) end;
  entry[2] = flags;

  ctx->len++;

  return 0;
}

static inline int
bare_type_stripper__add_range(bare_type_stripper_t *ctx, size_t start, size_t end) {
  return bare_type_stripper__add_range_flags(ctx, start, end, 0);
}

// True iff s[ks..ks+kl] equals the keyword `kw` of length `kwl`. Use to test
// an already-lexed identifier span against a known keyword without rescanning.
static inline bool
bare_type_stripper__match_kw(const utf8_t *s, size_t ks, size_t kl, const char *kw, size_t kwl) {
  return kl == kwl && memcmp(&s[ks], kw, kwl) == 0;
}

// True iff the bytes at i spell the keyword `kw` of length `kwl` with a word
// boundary after it (end of input or a non-identifier byte).
static inline bool
bare_type_stripper__at_kw(const utf8_t *s, size_t n, size_t i, const char *kw, size_t kwl) {
  return i + kwl <= n && memcmp(&s[i], kw, kwl) == 0 && (i + kwl == n || !bare_type_stripper__is_id(s[i + kwl]));
}

// True iff the identifier span s[ks..ks+kl] is one of the TS modifier
// keywords that can prefix a class member or parameter property: 'public',
// 'private', 'protected', 'readonly', 'override' - and 'abstract' when
// `allow_abstract` is true (class members can be abstract, parameters can't).
static inline bool
bare_type_stripper__is_ts_modifier(const utf8_t *s, size_t ks, size_t kl, bool allow_abstract) {
  if (kl == 6) return memcmp(&s[ks], "public", 6) == 0;
  if (kl == 7) return memcmp(&s[ks], "private", 7) == 0;
  if (kl == 8) {
    return memcmp(&s[ks], "readonly", 8) == 0 ||
           memcmp(&s[ks], "override", 8) == 0 ||
           (allow_abstract && memcmp(&s[ks], "abstract", 8) == 0);
  }
  if (kl == 9) return memcmp(&s[ks], "protected", 9) == 0;
  return false;
}

static inline size_t
bare_type_stripper__skip_string(const utf8_t *s, size_t n, size_t i);

static inline size_t
bare_type_stripper__skip_template(const utf8_t *s, size_t n, size_t i);

static inline size_t
bare_type_stripper__skip_regex(const utf8_t *s, size_t n, size_t i);

static inline size_t
bare_type_stripper__skip_trivia(const utf8_t *s, size_t n, size_t i);

static inline size_t
bare_type_stripper__scan_generic(const utf8_t *s, size_t n, size_t i);

static inline size_t
bare_type_stripper__scan_type(const utf8_t *s, size_t n, size_t i, uint32_t terms);

static inline size_t
bare_type_stripper__scan_declare(const utf8_t *s, size_t n, size_t i);

#define BARE_TYPE_STRIPPER__T_COMMA 0x01
#define BARE_TYPE_STRIPPER__T_SEMI  0x02
#define BARE_TYPE_STRIPPER__T_PAREN 0x04  // ')'
#define BARE_TYPE_STRIPPER__T_BRACE 0x08  // '{'/'}'
#define BARE_TYPE_STRIPPER__T_BRACK 0x10  // ']'
#define BARE_TYPE_STRIPPER__T_EQ    0x20  // '='
#define BARE_TYPE_STRIPPER__T_NL    0x40  // Bare newline
#define BARE_TYPE_STRIPPER__T_ANGLE 0x80  // '>' at depth 0
#define BARE_TYPE_STRIPPER__T_ARROW 0x100 // '=>' at depth 0
#define BARE_TYPE_STRIPPER__T_INOF  0x200 // 'in'/'of' identifier at depth 0

static inline size_t
bare_type_stripper__skip_trivia(const utf8_t *s, size_t n, size_t i) {
  while (i < n) {
    if (ws(u(0))) {
      i++;
    } else if (i + 1 < n && u(0) == '/' && u(1) == '/') {
      i += 2;
      while (i < n && !lt(u(0))) i++;
    } else if (i + 1 < n && u(0) == '/' && u(1) == '*') {
      i += 2;
      while (i + 1 < n && !(u(0) == '*' && u(1) == '/')) i++;
      if (i + 1 < n) i += 2;
      else i = n;
    } else {
      break;
    }
  }

  return i;
}

// Skip leading trivia, an identifier if present, and trailing trivia. Used
// to consume the name of a declaration ('class NAME', 'enum NAME', ...).
static inline size_t
bare_type_stripper__skip_name(const utf8_t *s, size_t n, size_t i) {
  i = bare_type_stripper__skip_trivia(s, n, i);
  while (i < n && id(u(0))) i++;
  return bare_type_stripper__skip_trivia(s, n, i);
}

// True when removing a chunk ending at 'pos' would expose an ASI hazard:
// The next non-trivia byte is across a newline and starts a token that would
// continue the previous expression (call '(', subscript '[', tagged template
// '`'). Used by 'as'/'satisfies' strips, where blanking the type otherwise
// glues 'foo as T\n(1)' into 'foo(1)'.
static inline bool
bare_type_stripper__asi_hazard_after(const utf8_t *s, size_t n, size_t i) {
  bool seen_nl = false;

  while (i < n) {
    uint8_t c = u(0);
    if (lt(c)) {
      seen_nl = true;
      i++;
      continue;
    }
    if (ws(c)) {
      i++;
      continue;
    }
    if (i + 1 < n && c == '/' && u(1) == '/') {
      i += 2;
      while (i < n && !lt(u(0))) i++;
      continue;
    }
    if (i + 1 < n && c == '/' && u(1) == '*') {
      i += 2;
      while (i + 1 < n && !(u(0) == '*' && u(1) == '/')) {
        if (lt(u(0))) seen_nl = true;
        i++;
      }
      if (i + 1 < n) i += 2;
      else i = n;
      continue;
    }
    return seen_nl && (c == '(' || c == '[' || c == '`');
  }

  return false;
}

static inline size_t
bare_type_stripper__skip_string(const utf8_t *s, size_t n, size_t i) {
  uint8_t e = u(0);
  i++;

  while (i < n) {
    if (u(0) == '\\') {
      if (i + 1 < n) i += 2;
      else i++;
      continue;
    }

    if (u(0) == e) {
      i++;
      break;
    }

    if (lt(u(0))) break;

    i++;
  }

  return i;
}

static inline size_t
bare_type_stripper__skip_template(const utf8_t *s, size_t n, size_t i) {
  i++;

  while (i < n) {
    if (u(0) == '\\') {
      if (i + 1 < n) i += 2;
      else i++;
      continue;
    }

    if (u(0) == '`') {
      i++;
      break;
    }

    if (u(0) == '$' && i + 1 < n && u(1) == '{') {
      i += 2;
      int depth = 1;

      while (i < n && depth > 0) {
        if (u(0) == '`') {
          i = bare_type_stripper__skip_template(s, n, i);
          continue;
        }
        if (u(0) == '\'' || u(0) == '"') {
          i = bare_type_stripper__skip_string(s, n, i);
          continue;
        }
        if (u(0) == '/' && i + 1 < n && (u(1) == '/' || u(1) == '*')) {
          i = bare_type_stripper__skip_trivia(s, n, i);
          continue;
        }

        if (u(0) == '{') depth++;
        else if (u(0) == '}') depth--;

        if (depth == 0) {
          i++;
          break;
        }

        i++;
      }

      continue;
    }

    i++;
  }

  return i;
}

static inline size_t
bare_type_stripper__skip_regex(const utf8_t *s, size_t n, size_t i) {
  i++;
  bool in_class = false;

  while (i < n) {
    if (u(0) == '\\') {
      if (i + 1 < n) i += 2;
      else i++;
      continue;
    }

    if (in_class) {
      if (u(0) == ']') in_class = false;
    } else {
      if (u(0) == '[') {
        in_class = true;
      } else if (u(0) == '/') {
        i++;
        while (i < n && u(0) >= 'a' && u(0) <= 'z') i++;
        return i;
      } else if (lt(u(0))) {
        return i;
      }
    }

    i++;
  }

  return i;
}

// Advance past a single span of inert content (whitespace, line comment,
// block comment, string literal, template literal) at *result. Returns true
// and updates *result if it consumed something; returns false otherwise.
// Used by balanced-bracket scanners that need to ignore brackets nested
// inside literals and comments.
static inline bool
bare_type_stripper__try_skip_inert(const utf8_t *s, size_t n, size_t *result) {
  size_t i = *result;
  if (i >= n) return false;

  uint8_t c = u(0);
  if (ws(c)) {
    *result = i + 1;
    return true;
  }
  if (c == '/' && i + 1 < n && (u(1) == '/' || u(1) == '*')) {
    *result = bare_type_stripper__skip_trivia(s, n, i);
    return true;
  }
  if (c == '\'' || c == '"') {
    *result = bare_type_stripper__skip_string(s, n, i);
    return true;
  }
  if (c == '`') {
    *result = bare_type_stripper__skip_template(s, n, i);
    return true;
  }
  return false;
}

// Scan past a balanced 'open ... close' span. Assumes s[i] == open. Returns
// the index just past the matching close. Skips over comments, strings, and
// template literals so brackets inside them don't affect the depth count.
// If the brackets never close, returns n.
static inline size_t
bare_type_stripper__scan_balanced(const utf8_t *s, size_t n, size_t i, uint8_t open, uint8_t close) {
  if (i >= n || u(0) != open) return i + 1;
  i++;
  int depth = 1;

  while (i < n && depth > 0) {
    if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

    if (u(0) == open) depth++;
    else if (u(0) == close) depth--;

    i++;
  }

  return i;
}

// Scan past a balanced '(...)' group, like scan_balanced, but backed by the
// context's paren match cache. The arrow-function lookahead runs this for
// every candidate '(' - on nested groups the inner scans would otherwise
// redo the outer scan's work, making the lookahead quadratic in nesting
// depth. Assumes s[i] == '('.
static inline size_t
bare_type_stripper__scan_group(bare_type_stripper_t *ctx, size_t i) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  size_t slot = i & (BARE_TYPE_STRIPPER__PAREN_CACHE_SIZE - 1);
  if (ctx->paren_close[slot] != 0 && ctx->paren_open[slot] == i) return ctx->paren_close[slot];

  // Positions of the unclosed '(' at each depth, so every nested match
  // discovered along the way gets cached as well. Levels deeper than the
  // stack still balance, they just aren't recorded.
  size_t opens[BARE_TYPE_STRIPPER__PAREN_STACK_SIZE];
  int depth = 0;

  opens[depth++] = i;
  i++;

  while (i < n && depth > 0) {
    if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

    if (u(0) == '(') {
      if (depth < BARE_TYPE_STRIPPER__PAREN_STACK_SIZE) opens[depth] = i;
      depth++;
      i++;
      continue;
    }

    if (u(0) == ')') {
      i++;
      depth--;

      if (depth < BARE_TYPE_STRIPPER__PAREN_STACK_SIZE) {
        size_t open = opens[depth];
        size_t slot = open & (BARE_TYPE_STRIPPER__PAREN_CACHE_SIZE - 1);

        ctx->paren_open[slot] = open;
        ctx->paren_close[slot] = i;
      }

      continue;
    }

    i++;
  }

  return i;
}

// Scan past a balanced '<...>'. Assumes s[i] == '<'. Returns the index after
// the closing '>'. If the brackets don't balance cleanly (e.g. we ran into a
// ';' which is almost certainly a comparison, not a generic), returns the
// original i+1 to signal "not a generic".
static inline size_t
bare_type_stripper__scan_generic(const utf8_t *s, size_t n, size_t i) {
  size_t start = i;
  if (u(0) != '<') return i + 1;

  i++;
  int depth = 1;

  while (i < n && depth > 0) {
    if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

    uint8_t ch = u(0);

    if (ch == '<') {
      depth++;
      i++;
      continue;
    }
    if (ch == '>') {
      depth--;
      i++;
      continue;
    }

    // '=>' is an arrow-type separator inside the generic arguments (e.g.
    // 'foo<(a: T) => U>()'), not a closing '>'.
    if (ch == '=' && i + 1 < n && u(1) == '>') {
      i += 2;
      continue;
    }

    if (ch == '(' || ch == '[' || ch == '{') {
      uint8_t close = ch == '(' ? ')' : ch == '[' ? ']'
                                                  : '}';
      i = bare_type_stripper__scan_balanced(s, n, i, ch, close);
      continue;
    }

    // ';' or an unmatched closing bracket can't appear inside generic
    // arguments: This is almost certainly a comparison.
    if (ch == ';' || ch == ')' || ch == ']' || ch == '}') return start + 1;

    i++;
  }

  // The brackets never balanced before end of input. Signal "not a generic"
  // rather than returning n, which callers would mistake for a generic
  // spanning the rest of the file.
  if (depth > 0) return start + 1;

  return i;
}

// Strip a balanced '<...>' generic argument or parameter list at *result,
// if present. Advances past it on success; leaves the cursor alone when
// there is no '<' or the brackets don't scan as a generic.
static inline int
bare_type_stripper__strip_generic(bare_type_stripper_t *ctx, size_t *result) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  size_t i = *result;

  if (i >= n || s[i] != '<') return 0;

  size_t ge = bare_type_stripper__scan_generic(s, n, i);
  if (ge <= i + 1) return 0;

  int err = bare_type_stripper__add_range(ctx, i, ge);
  if (err < 0) return err;

  *result = ge;
  return 0;
}

// Scan past a type expression starting at i. Terminates at the first
// character listed in `terms` at depth 0. Balances (), [], {}, <>.
//
// At depth 0 with T_NL, a newline terminates the scan only if the type
// doesn't appear to continue across it - i.e. the previous token wasn't a
// type-level operator AND the next non-trivia token isn't one either. This
// keeps multi-line union/intersection/conditional types intact while still
// honouring ASI at the end of an actual statement.
static inline size_t
bare_type_stripper__scan_type(const utf8_t *s, size_t n, size_t i, uint32_t terms) {
  int depth = 0;
  bool seen_token = false;
  // True when the most recent significant token was an operator that
  // expects another type expression to follow (e.g. '|', '&', 'extends').
  // Starts true so an immediate leading newline doesn't stop us.
  bool last_was_op = true;

  while (i < n) {
    uint8_t ch = u(0);

    if (ws(ch)) {
      if (depth == 0 && (terms & BARE_TYPE_STRIPPER__T_NL) && lt(ch)) {
        bool continues = last_was_op;
        if (!continues) {
          size_t peek = bare_type_stripper__skip_trivia(s, n, i + 1);
          if (peek < n) {
            uint8_t nb = s[peek];
            if (nb == '|' || nb == '&' || nb == '?' || nb == ':' || nb == '.') {
              continues = true;
            }
          }
        }
        if (!continues) return i;
      }
      i++;
      continue;
    }
    if (ch == '/' && i + 1 < n && (u(1) == '/' || u(1) == '*')) {
      i = bare_type_stripper__skip_trivia(s, n, i);
      continue;
    }
    if (ch == '\'' || ch == '"') {
      i = bare_type_stripper__skip_string(s, n, i);
      seen_token = true;
      last_was_op = false;
      continue;
    }
    if (ch == '`') {
      i = bare_type_stripper__skip_template(s, n, i);
      seen_token = true;
      last_was_op = false;
      continue;
    }

    // '=>' inside brackets is part of an arrow type - consume it whole so
    // its '>' can't be mistaken for the close of an enclosing '<'.
    if (ch == '=' && depth > 0 && i + 1 < n && u(1) == '>') {
      i += 2;
      seen_token = true;
      last_was_op = true;
      continue;
    }

    if (depth == 0) {
      if ((terms & BARE_TYPE_STRIPPER__T_COMMA) && ch == ',') return i;
      if ((terms & BARE_TYPE_STRIPPER__T_SEMI) && ch == ';') return i;
      if ((terms & BARE_TYPE_STRIPPER__T_PAREN) && ch == ')') return i;
      if ((terms & BARE_TYPE_STRIPPER__T_BRACK) && ch == ']') return i;
      if ((terms & BARE_TYPE_STRIPPER__T_BRACE) && ch == '}') return i;
      if ((terms & BARE_TYPE_STRIPPER__T_ANGLE) && ch == '>') return i;

      if (ch == '=') {
        // '==' is comparison (rare inside a type, treated as continuation).
        if (i + 1 < n && u(1) == '=') {
          i += 2;
          seen_token = true;
          last_was_op = true;
          continue;
        }

        // '=>' is either an arrow-function separator (terminator) or part of
        // an arrow-type expression (continuation), depending on terms.
        if (i + 1 < n && u(1) == '>') {
          if (terms & BARE_TYPE_STRIPPER__T_ARROW) return i;
          i += 2;
          seen_token = true;
          last_was_op = true;
          continue;
        }

        if (terms & BARE_TYPE_STRIPPER__T_EQ) return i;

        i++;
        seen_token = true;
        last_was_op = true;
        continue;
      }

      // A '{' directly after a type-level operator ('&', '|', 'extends',
      // ...) is an object type literal operand, not the '{' the caller is
      // waiting for - only stop on it after a complete type.
      if ((terms & BARE_TYPE_STRIPPER__T_BRACE) && ch == '{' && seen_token && !last_was_op) return i;
    }

    if (ch == '(' || ch == '[' || ch == '{') {
      depth++;
      i++;
      seen_token = true;
      last_was_op = true;
      continue;
    }
    if (ch == ')' || ch == ']' || ch == '}') {
      if (depth == 0) return i;
      depth--;
      i++;
      last_was_op = false;
      continue;
    }

    if (ch == '<') {
      depth++;
      i++;
      seen_token = true;
      last_was_op = true;
      continue;
    }
    if (ch == '>') {
      if (depth > 0) {
        depth--;
        i++;
        last_was_op = false;
        continue;
      }
      // At depth 0, '>' is a comparison operator in surrounding JS code, not
      // a type close - it ends the type. Without this, 'as number > 2' would
      // greedily swallow ' > 2' into the cast.
      return i;
    }

    if (ids(ch)) {
      size_t ks = i;
      while (i < n && id(u(0))) i++;
      size_t kl = i - ks;

      if (depth == 0 && (terms & BARE_TYPE_STRIPPER__T_INOF)) {
        if (kl == 2 && (memcmp(&s[ks], "in", 2) == 0 || memcmp(&s[ks], "of", 2) == 0)) {
          return ks;
        }
      }

      // Type-level keywords that expect another type to follow. Marking these
      // as 'op' keeps multi-line conditional types intact (e.g. 'T extends\n
      // U ? A : B').
      last_was_op =
        (kl == 2 && memcmp(&s[ks], "is", 2) == 0) ||
        (kl == 5 && (memcmp(&s[ks], "infer", 5) == 0 ||
                     memcmp(&s[ks], "keyof", 5) == 0)) ||
        (kl == 6 && memcmp(&s[ks], "typeof", 6) == 0) ||
        (kl == 7 && (memcmp(&s[ks], "extends", 7) == 0 ||
                     memcmp(&s[ks], "asserts", 7) == 0));
      seen_token = true;
      continue;
    }

    // Punctuation that combines two type expressions ('|', '&', '?', ':', ',').
    // We don't terminate on these here (T_COMMA above already covers ',' at
    // depth 0 when set, and ',' inside brackets has depth > 0); they just
    // continue scanning and mark the position as 'after an operator'.
    last_was_op = (ch == '|' || ch == '&' || ch == '?' || ch == ':' || ch == ',' || ch == '.');
    seen_token = true;
    i++;
  }

  return i;
}

// Forward declaration of the master walker - used by helpers that process
// nested blocks (function bodies, class methods, etc.).
static inline int
bare_type_stripper__walk(bare_type_stripper_t *ctx, size_t *result, uint8_t stop_at, bool body_is_stmt, bool stop_at_asi, uint32_t depth);

// Forward declaration of the function processor - used by the export
// processor to delegate 'export [default] function ...' statements.
static inline int
bare_type_stripper__process_function(bare_type_stripper_t *ctx, size_t *result, size_t stmt_start, uint32_t depth);

// Walk a template literal, recursively stripping type syntax inside each
// '${...}' expression. Assumes s[*result] == '`'. Leaves the cursor after
// the closing '`'. Unlike skip_template (which only advances the cursor),
// this invokes the walker on the substitution expressions so 'x as Foo'
// inside '${x as Foo}' still gets stripped.
static inline int
bare_type_stripper__walk_template(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;
  i++;

  while (i < n) {
    if (u(0) == '\\') {
      if (i + 1 < n) i += 2;
      else i++;
      continue;
    }

    if (u(0) == '`') {
      i++;
      break;
    }

    if (u(0) == '$' && i + 1 < n && u(1) == '{') {
      i += 2;

      err = bare_type_stripper__walk(ctx, &i, '}', false, false, depth + 1);
      if (err < 0) return err;

      continue;
    }

    i++;
  }

  *result = i;
  return 0;
}

// Process a decorator '@...'. Expects s[i] == '@'. Handles three forms:
//
//   @Name.Path                  bare reference
//   @Name.Path<T>               TS 4.7+ generic decorator (strips '<T>')
//   @Name.Path(args)            decorator factory call (walks args)
//   @(expr)                     parenthesized expression (walks expr)
//
// In the (args) and (expr) forms we recurse via walk() so 'as'/'satisfies'/
// '!' inside the args also get stripped. Leaves the cursor just past the
// closing ')' if there is one, otherwise just past the identifier path.
static inline int
bare_type_stripper__process_decorator(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  if (i >= n || u(0) != '@') return 0;
  i++;

  if (i < n && u(0) == '(') {
    i++;

    err = bare_type_stripper__walk(ctx, &i, ')', false, false, depth + 1);
    if (err < 0) return err;

    if (i < n && u(0) == ')') i++;
  } else {
    while (i < n && (id(u(0)) || u(0) == '.')) i++;

    err = bare_type_stripper__strip_generic(ctx, &i);
    if (err < 0) return err;

    i = bare_type_stripper__skip_trivia(s, n, i);

    if (i < n && u(0) == '(') {
      i++;

      err = bare_type_stripper__walk(ctx, &i, ')', false, false, depth + 1);
      if (err < 0) return err;

      if (i < n && u(0) == ')') i++;
    }
  }

  *result = i;
  return 0;
}

// Process a function's parameter list. Expects s[i] == '('. Strips parameter
// modifiers (parameter properties), type annotations and optional '?' markers.
static inline int
bare_type_stripper__process_params(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  if (u(0) != '(') {
    *result = i;
    return 0;
  }
  i++;

  // 'this: T' parameter. TypeScript allows the first parameter to be named
  // 'this' for typing the call context; it has no runtime presence and must
  // be erased entirely (including its trailing comma if more params follow).
  size_t after = bare_type_stripper__skip_trivia(s, n, i);
  if (bare_type_stripper__at_kw(s, n, after, "this", 4)) {
    size_t this_end = after + 4;
    size_t scan = bare_type_stripper__skip_trivia(s, n, this_end);

    if (scan < n && (s[scan] == ':' || s[scan] == ',' || s[scan] == ')')) {
      // Walk past the optional ': T' annotation.
      if (s[scan] == ':') {
        scan++;
        scan = bare_type_stripper__scan_type(s, n, scan, BARE_TYPE_STRIPPER__T_COMMA | BARE_TYPE_STRIPPER__T_PAREN);
      }

      // Erase the 'this' parameter and the trailing comma when more
      // parameters follow; if not, leave the closing ')' for the loop.
      size_t strip_end = scan;
      if (scan < n && s[scan] == ',') strip_end = scan + 1;

      err = bare_type_stripper__add_range(ctx, i, strip_end);
      if (err < 0) return err;
      i = strip_end;
    }
  }

  // Each iteration recognizes one feature (a modifier, a name, an optional
  // marker, an annotation, a default, a comma, etc.) and 'continue's. Any
  // unrecognized byte falls through to the bottom 'i++', which guarantees
  // forward progress on malformed input without an explicit safeguard.
  while (i < n) {
    uint8_t ch = u(0);

    if (ws(ch) || (ch == '/' && i + 1 < n && (u(1) == '/' || u(1) == '*'))) {
      i = bare_type_stripper__skip_trivia(s, n, i);
      continue;
    }

    if (ch == ')') {
      i++;
      *result = i;
      return 0;
    }

    if (ch == ',') {
      i++;
      continue;
    }

    // Decorator on a parameter - leave the decorator itself alone (decorators
    // emit runtime code) but walk its args so any TS inside gets stripped.
    if (ch == '@') {
      err = bare_type_stripper__process_decorator(ctx, &i, depth);
      if (err < 0) return err;
      continue;
    }

    // Rest parameter '...'
    if (i + 2 < n && ch == '.' && u(1) == '.' && u(2) == '.') {
      i += 3;
      continue;
    }

    // Destructuring pattern. Balance brackets but don't recurse - destructuring
    // may contain nested type annotations only in obscure cases.
    if (ch == '[' || ch == '{') {
      uint8_t close = ch == '[' ? ']' : '}';
      i = bare_type_stripper__scan_balanced(s, n, i, ch, close);
      continue;
    }

    // Optional marker '?'
    if (ch == '?') {
      err = bare_type_stripper__add_range(ctx, i, i + 1);
      if (err < 0) return err;

      i++;
      continue;
    }

    // Type annotation ': T'
    if (ch == ':') {
      size_t ts = i;
      i++;
      i = bare_type_stripper__scan_type(
        s,
        n,
        i,
        BARE_TYPE_STRIPPER__T_COMMA | BARE_TYPE_STRIPPER__T_PAREN | BARE_TYPE_STRIPPER__T_EQ
      );

      err = bare_type_stripper__add_range(ctx, ts, i);
      if (err < 0) return err;
      continue;
    }

    // Default value '=' - walk the expression, stopping at ',' or ')' at
    // depth 0 (the loop will see whichever stop char comes next).
    if (ch == '=') {
      i++;
      err = bare_type_stripper__walk(ctx, &i, ',', false, false, depth + 1);
      if (err < 0) return err;
      continue;
    }

    // Identifier - either a TS modifier (public/private/protected/readonly/
    // override) prefixing a parameter property, or the parameter name.
    if (ids(ch)) {
      size_t ks = i;
      while (i < n && id(u(0))) i++;
      size_t ke = i;
      size_t kl = ke - ks;

      if (bare_type_stripper__is_ts_modifier(s, ks, kl, false)) {
        size_t peek = bare_type_stripper__skip_trivia(s, n, ke);

        // Match the modifier only if followed by another modifier, an
        // identifier, or a destructuring/rest opener - i.e. we're really at
        // a parameter property and not just an identifier that happens to
        // spell a modifier keyword. A parameter property is non-erasable:
        // TypeScript assigns the parameter to a same-named instance member
        // at construction time, which stripping the modifier can't
        // reproduce, so mark it instead of erasing it.
        if (peek < n && (ids(s[peek]) || s[peek] == '[' || s[peek] == '{' || (s[peek] == '.' && peek + 2 < n && s[peek + 1] == '.' && s[peek + 2] == '.'))) {
          err = bare_type_stripper__add_range_flags(ctx, ks, ke, bare_type_stripper_error);
          if (err < 0) return err;
          i = peek;
        }
      }
      continue;
    }

    // Unrecognized byte - skip it so the loop can't wedge on malformed input.
    i++;
  }

  *result = i;
  return 0;
}

// Process the body of a class. Expects s[i] == '{'. Walks until matching '}'.
static inline int
bare_type_stripper__process_class_body(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  if (u(0) != '{') return 0;
  i++;

  while (i < n) {
    i = bare_type_stripper__skip_trivia(s, n, i);
    if (i >= n) break;

    if (u(0) == '}') {
      i++;
      *result = i;
      return 0;
    }
    if (u(0) == ';') {
      i++;
      continue;
    }

    // Decorator on a member.
    if (u(0) == '@') {
      err = bare_type_stripper__process_decorator(ctx, &i, depth);
      if (err < 0) return err;
      continue;
    }

    // Remember where this member starts and how many ranges were emitted
    // before it. A member that turns out to be a bodyless overload or
    // abstract signature is rolled back to this point and erased whole.
    size_t member_start = i;
    uint32_t member_mark = ctx->len;

    // 'static' is a real JS keyword that prefixes a class member, or starts
    // a static initialization block. Consume it when followed by another
    // identifier (or '['/'*') and remember that the line now has a syntactic
    // anchor: A subsequent TS modifier strip on the same line must NOT inject
    // ';' since 'static' already breaks any ASI hazard with the preceding
    // member ('f = 1\nstatic readonly x' parses cleanly, but 'f = 1\nstatic ;
    // x' would not). When followed by '{', recurse into the block as plain
    // JS - the contents are statements, not class members.
    bool kept_jskw = false;
    if (bare_type_stripper__at_kw(s, n, i, "static", 6)) {
      size_t peek = bare_type_stripper__skip_trivia(s, n, i + 6);

      if (peek < n && s[peek] == '{') {
        i = peek;

        err = bare_type_stripper__walk(ctx, &i, '}', true, false, depth + 1);
        if (err < 0) return err;

        continue;
      }

      if (peek < n && (ids(s[peek]) || s[peek] == '[' || s[peek] == '*')) {
        i = i + 6;
        kept_jskw = true;
      }
    }

    // Track whether we've already emitted a strip range that starts this
    // class member. The first such range gets the F_SEMI flag so that ASI
    // can't merge the preceding field's initializer into this one (e.g.
    // 'field = 1\npublic ["k"] = 2' would parse as '1["k"]' after stripping
    // 'public' without an injected ';').
    bool member_strip_seen = kept_jskw;

    // Strip TS modifier keywords. Loop as long as we keep seeing modifiers
    // followed by another modifier or a member name.
    for (;;) {
      i = bare_type_stripper__skip_trivia(s, n, i);
      if (i >= n || !ids(u(0))) break;

      size_t ks = i;
      while (i < n && id(u(0))) i++;
      size_t ke = i;
      size_t kl = ke - ks;

      bool is_mod = bare_type_stripper__is_ts_modifier(s, ks, kl, true);

      if (!is_mod) {
        i = ks;
        break;
      }

      size_t peek = bare_type_stripper__skip_trivia(s, n, ke);

      // Erase modifier only if followed by another identifier (member name
      // or another modifier), '#' (private name), '[' (computed name), '*'
      // (generator), or a string or numeric member name.
      if (peek < n && (ids(s[peek]) || s[peek] == '#' || s[peek] == '[' || s[peek] == '*' || s[peek] == '\'' || s[peek] == '"' || (s[peek] >= '0' && s[peek] <= '9'))) {
        uint32_t flags = member_strip_seen ? 0 : bare_type_stripper_semi;

        err = bare_type_stripper__add_range_flags(ctx, ks, peek, flags);
        if (err < 0) return err;

        member_strip_seen = true;
        i = peek;
        continue;
      }

      i = ks;
      break;
    }

    // 'declare' member - erase to end of statement. scan_declare balances
    // brackets, so an object type in the annotation can't end the strip at
    // its inner '}'.
    if (bare_type_stripper__at_kw(s, n, i, "declare", 7)) {
      size_t ds = i;
      i = bare_type_stripper__scan_declare(s, n, i + 7);

      uint32_t flags = member_strip_seen ? 0 : bare_type_stripper_semi;

      err = bare_type_stripper__add_range_flags(ctx, ds, i, flags);
      if (err < 0) return err;

      continue;
    }

    // Index signature '[k: K]: V'
    if (u(0) == '[') {
      size_t bs = i;
      i = bare_type_stripper__scan_balanced(s, n, i, '[', ']');

      size_t after = bare_type_stripper__skip_trivia(s, n, i);
      if (after < n && s[after] == ':') {
        // Index signature - erase entire signature.
        i = after + 1;
        i = bare_type_stripper__scan_type(
          s,
          n,
          i,
          BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_NL | BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_COMMA
        );
        if (i < n && (u(0) == ';' || u(0) == ',')) i++;

        err = bare_type_stripper__add_range(ctx, bs, i);
        if (err < 0) return err;

        continue;
      }

      // Otherwise it's a computed member name - rewind and let the member
      // name parsing below consume it.
      i = bs;
    }

    // 'get' / 'set' accessor prefix. Keep it, but consume it here rather
    // than letting it lex as the member name, so that the member's real
    // name and signature are processed in this same iteration.
    if (bare_type_stripper__at_kw(s, n, i, "get", 3) || bare_type_stripper__at_kw(s, n, i, "set", 3)) {
      size_t peek = bare_type_stripper__skip_trivia(s, n, i + 3);
      if (peek < n && (ids(s[peek]) || s[peek] == '[' || s[peek] == '#' || s[peek] == '\'' || s[peek] == '"' || (s[peek] >= '0' && s[peek] <= '9'))) {
        i = peek;
      }
    }

    // Member name.
    if (u(0) == '*') {
      i++;
      i = bare_type_stripper__skip_trivia(s, n, i);
    }
    if (u(0) == '#') i++; // Private name

    if (i < n && ids(u(0))) {
      while (i < n && id(u(0))) i++;
    } else if (u(0) == '\'' || u(0) == '"') {
      i = bare_type_stripper__skip_string(s, n, i);
    } else if (u(0) == '[') {
      // Computed member name.
      i = bare_type_stripper__scan_balanced(s, n, i, '[', ']');
    } else if (u(0) >= '0' && u(0) <= '9') {
      while (i < n && (id(u(0)) || u(0) == '.')) i++;
    } else {
      // Unknown - advance one byte to recover.
      i++;
      continue;
    }

    i = bare_type_stripper__skip_trivia(s, n, i);

    // Optional '?' or definite assignment '!:' on a property.
    if (i < n && u(0) == '?') {
      size_t peek = bare_type_stripper__skip_trivia(s, n, i + 1);
      if (peek < n && (s[peek] == ':' || s[peek] == '(' || s[peek] == ';' || s[peek] == '=' || s[peek] == '}' || lt(s[peek]) || s[peek] == ',')) {
        err = bare_type_stripper__add_range(ctx, i, i + 1);
        if (err < 0) return err;

        i++;
        i = bare_type_stripper__skip_trivia(s, n, i);
      }
    } else if (i < n && u(0) == '!') {
      size_t peek = bare_type_stripper__skip_trivia(s, n, i + 1);
      if (peek < n && s[peek] == ':') {
        err = bare_type_stripper__add_range(ctx, i, i + 1);
        if (err < 0) return err;

        i++;
        i = bare_type_stripper__skip_trivia(s, n, i);
      }
    }

    // Method generics '<T>'.
    if (i < n && u(0) == '<') {
      size_t gs = i;
      size_t ge = bare_type_stripper__scan_generic(s, n, i);

      if (ge > i + 1) {
        size_t peek = bare_type_stripper__skip_trivia(s, n, ge);
        if (peek < n && s[peek] == '(') {
          err = bare_type_stripper__add_range(ctx, gs, ge);
          if (err < 0) return err;

          i = ge;
          i = bare_type_stripper__skip_trivia(s, n, i);
        }
      }
    }

    if (i >= n) break;

    if (u(0) == '(') {
      // Method.
      err = bare_type_stripper__process_params(ctx, &i, depth);
      if (err < 0) return err;

      i = bare_type_stripper__skip_trivia(s, n, i);

      // Return type annotation.
      if (i < n && u(0) == ':') {
        size_t ts = i;
        i++;
        i = bare_type_stripper__scan_type(
          s,
          n,
          i,
          BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_NL
        );

        err = bare_type_stripper__add_range(ctx, ts, i);
        if (err < 0) return err;

        i = bare_type_stripper__skip_trivia(s, n, i);
      }

      // Method body. Without one this is an overload or abstract method
      // signature, which has no runtime semantics: Drop the ranges emitted
      // for its pieces and erase the whole member as one range instead -
      // leaving a bodyless 'f()' behind would produce invalid runtime JS.
      // The injected ';' keeps ASI from folding a preceding field
      // initializer into whatever follows the erased member.
      if (i < n && u(0) == '{') {
        err = bare_type_stripper__walk(ctx, &i, '}', true, false, depth + 1);
        if (err < 0) return err;
      } else {
        if (i < n && u(0) == ';') i++;

        ctx->len = member_mark;

        err = bare_type_stripper__add_range_flags(ctx, member_start, i, bare_type_stripper_semi);
        if (err < 0) return err;
      }

      continue;
    }

    if (u(0) == ':') {
      // Property type annotation.
      size_t ts = i;
      i++;
      i = bare_type_stripper__scan_type(
        s,
        n,
        i,
        BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_EQ | BARE_TYPE_STRIPPER__T_NL | BARE_TYPE_STRIPPER__T_BRACE
      );

      err = bare_type_stripper__add_range(ctx, ts, i);
      if (err < 0) return err;

      i = bare_type_stripper__skip_trivia(s, n, i);
    }

    // Initializer. Pass stop_at_asi so the walker stops at a newline at
    // depth 0 (after an expression-ending token) - otherwise it would chew
    // past the next class element when the field has no trailing ';'.
    if (i < n && u(0) == '=') {
      i++;

      err = bare_type_stripper__walk(ctx, &i, ';', false, true, depth + 1);
      if (err < 0) return err;

      if (i < n && u(0) == ';') i++;
    }
  }

  *result = i;
  return 0;
}

// True when the property starting at i parses as a method head - i.e.
// '[async] [*] [get|set] KEY [<...>]' directly followed by '(' - meaning
// the property is a shorthand method whose signature needs type stripping.
// Pure lookahead: Consumes nothing and emits no ranges.
static inline bool
bare_type_stripper__at_method_head(const utf8_t *s, size_t n, size_t i) {
  // 'async' prefix.
  if (bare_type_stripper__at_kw(s, n, i, "async", 5)) {
    size_t p = bare_type_stripper__skip_trivia(s, n, i + 5);
    if (p < n && (ids(s[p]) || s[p] == '*' || s[p] == '[' || s[p] == '\'' || s[p] == '"' || (s[p] >= '0' && s[p] <= '9'))) i = p;
  }

  // Generator marker.
  if (i < n && s[i] == '*') i = bare_type_stripper__skip_trivia(s, n, i + 1);

  // 'get' / 'set' accessor prefix.
  if (bare_type_stripper__at_kw(s, n, i, "get", 3) || bare_type_stripper__at_kw(s, n, i, "set", 3)) {
    size_t p = bare_type_stripper__skip_trivia(s, n, i + 3);
    if (p < n && (ids(s[p]) || s[p] == '[' || s[p] == '\'' || s[p] == '"' || (s[p] >= '0' && s[p] <= '9'))) i = p;
  }

  // Key.
  if (i < n && ids(s[i])) {
    while (i < n && id(s[i])) i++;
  } else if (i < n && (s[i] == '\'' || s[i] == '"')) {
    i = bare_type_stripper__skip_string(s, n, i);
  } else if (i < n && s[i] == '[') {
    i = bare_type_stripper__scan_balanced(s, n, i, '[', ']');
  } else if (i < n && s[i] >= '0' && s[i] <= '9') {
    while (i < n && (id(s[i]) || s[i] == '.')) i++;
  } else {
    return false;
  }

  i = bare_type_stripper__skip_trivia(s, n, i);

  // Method generics '<T>'.
  if (i < n && s[i] == '<') {
    size_t ge = bare_type_stripper__scan_generic(s, n, i);
    if (ge <= i + 1) return false;
    i = bare_type_stripper__skip_trivia(s, n, ge);
  }

  return i < n && s[i] == '(';
}

// Process the body of an object literal. Expects s[i] == '{'. Walks until
// the matching '}'. Properties are walked as plain expression code, except
// shorthand methods, whose signatures (generics, parameter annotations,
// return types) are stripped like class methods.
static inline int
bare_type_stripper__process_object_literal(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  if (u(0) != '{') return 0;
  i++;

  while (i < n) {
    i = bare_type_stripper__skip_trivia(s, n, i);
    if (i >= n) break;

    if (u(0) == '}') {
      i++;
      break;
    }
    if (u(0) == ',') {
      i++;
      continue;
    }
    if (u(0) == ')') {
      // Unmatched ')' - skip it so the loop can't wedge on malformed input.
      i++;
      continue;
    }

    if (bare_type_stripper__at_method_head(s, n, i)) {
      // 'async' prefix.
      if (bare_type_stripper__at_kw(s, n, i, "async", 5)) {
        size_t peek = bare_type_stripper__skip_trivia(s, n, i + 5);
        if (peek < n && (ids(s[peek]) || s[peek] == '*' || s[peek] == '[' || s[peek] == '\'' || s[peek] == '"' || (s[peek] >= '0' && s[peek] <= '9'))) {
          i = peek;
        }
      }

      // Generator marker.
      if (u(0) == '*') {
        i = bare_type_stripper__skip_trivia(s, n, i + 1);
      }

      // 'get' / 'set' accessor prefix.
      if (bare_type_stripper__at_kw(s, n, i, "get", 3) || bare_type_stripper__at_kw(s, n, i, "set", 3)) {
        size_t peek = bare_type_stripper__skip_trivia(s, n, i + 3);
        if (peek < n && (ids(s[peek]) || s[peek] == '[' || s[peek] == '\'' || s[peek] == '"' || (s[peek] >= '0' && s[peek] <= '9'))) {
          i = peek;
        }
      }

      // Key.
      if (i < n && ids(u(0))) {
        while (i < n && id(u(0))) i++;
      } else if (i < n && (u(0) == '\'' || u(0) == '"')) {
        i = bare_type_stripper__skip_string(s, n, i);
      } else if (i < n && u(0) == '[') {
        i = bare_type_stripper__scan_balanced(s, n, i, '[', ']');
      } else if (i < n && u(0) >= '0' && u(0) <= '9') {
        while (i < n && (id(u(0)) || u(0) == '.')) i++;
      }

      i = bare_type_stripper__skip_trivia(s, n, i);

      // Method generics '<T>'.
      err = bare_type_stripper__strip_generic(ctx, &i);
      if (err < 0) return err;

      i = bare_type_stripper__skip_trivia(s, n, i);

      // Parameter list.
      if (i < n && u(0) == '(') {
        err = bare_type_stripper__process_params(ctx, &i, depth);
        if (err < 0) return err;
      }

      i = bare_type_stripper__skip_trivia(s, n, i);

      // Return type annotation.
      if (i < n && u(0) == ':') {
        size_t ts = i;
        i++;
        i = bare_type_stripper__scan_type(
          s,
          n,
          i,
          BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_NL
        );

        err = bare_type_stripper__add_range(ctx, ts, i);
        if (err < 0) return err;

        i = bare_type_stripper__skip_trivia(s, n, i);
      }

      // Method body.
      if (i < n && u(0) == '{') {
        err = bare_type_stripper__walk(ctx, &i, '}', true, false, depth + 1);
        if (err < 0) return err;
      }

      continue;
    }

    // Anything else - spread, shorthand, or 'key: value' - is plain
    // expression code up to the next depth-0 ',' or the closing '}'.
    err = bare_type_stripper__walk(ctx, &i, ',', false, false, depth + 1);
    if (err < 0) return err;
  }

  *result = i;
  return 0;
}

// Scan past an 'enum IDENT { ... }' construct. Assumes s[i] points at the
// 'enum' keyword. Returns the index after the matching '}'. Used to mark
// the whole construct as a non-erasable range.
static inline size_t
bare_type_stripper__scan_enum(const utf8_t *s, size_t n, size_t i) {
  i = bare_type_stripper__skip_name(s, n, i + 4);

  if (i < n && u(0) == '{') {
    i = bare_type_stripper__scan_balanced(s, n, i, '{', '}');
  }

  return i;
}

// Scan past an 'interface IDENT [<...>] [extends ...] { ... }' construct
// without emitting any strip ranges. Assumes s[i] points at 'interface'.
// Returns the index after the matching '}'.
static inline size_t
bare_type_stripper__scan_interface(const utf8_t *s, size_t n, size_t i) {
  i = bare_type_stripper__skip_name(s, n, i + 9);

  if (i < n && u(0) == '<') {
    i = bare_type_stripper__scan_generic(s, n, i);
    i = bare_type_stripper__skip_trivia(s, n, i);
  }

  // Optional 'extends X, Y, ...'.
  if (bare_type_stripper__at_kw(s, n, i, "extends", 7)) {
    i += 7;
    int depth = 0;

    while (i < n) {
      if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

      uint8_t cc = u(0);

      if (cc == '<' || cc == '(' || cc == '[') {
        depth++;
        i++;
        continue;
      }
      if (cc == '>' || cc == ')' || cc == ']') {
        if (depth == 0) break;
        depth--;
        i++;
        continue;
      }
      if (cc == '{' && depth == 0) break;

      i++;
    }
  }

  if (i < n && u(0) == '{') {
    i = bare_type_stripper__scan_balanced(s, n, i, '{', '}');
  }

  return i;
}

// Process a top-level 'interface IDENT [...] { ... }' declaration. Emits a
// single SEMI-flagged range covering the whole construct.
static inline int
bare_type_stripper__process_interface(bare_type_stripper_t *ctx, size_t *result) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t start = *result;
  size_t end = bare_type_stripper__scan_interface(s, n, start);

  err = bare_type_stripper__add_range_flags(ctx, start, end, bare_type_stripper_semi);
  if (err < 0) return err;

  *result = end;
  return 0;
}

// Scan past the remainder of a 'type IDENT [<...>] = T;' alias. On entry, i
// points just past the 'type' keyword. Returns the index past the alias and
// its terminating ';' when present.
static inline size_t
bare_type_stripper__scan_type_alias(const utf8_t *s, size_t n, size_t i) {
  i = bare_type_stripper__skip_name(s, n, i);

  if (i < n && u(0) == '<') {
    i = bare_type_stripper__scan_generic(s, n, i);
    i = bare_type_stripper__skip_trivia(s, n, i);
  }

  if (i < n && u(0) == '=') {
    i++;
    i = bare_type_stripper__scan_type(s, n, i, BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_NL);
  }

  if (i < n && u(0) == ';') i++;

  return i;
}

// Process 'type IDENT [<...>] = T;'. Emits the whole declaration as a range.
static inline int
bare_type_stripper__process_type_alias(bare_type_stripper_t *ctx, size_t *result) {
  int err;
  size_t start = *result;
  size_t end = bare_type_stripper__scan_type_alias(ctx->s, ctx->n, start + 4);

  err = bare_type_stripper__add_range_flags(ctx, start, end, bare_type_stripper_semi);
  if (err < 0) return err;

  *result = end;
  return 0;
}

// Scan past the remainder of a 'declare ...' construct. On entry, i points
// just past the 'declare' keyword. Returns the index past the declared body
// or the terminating ';' / newline.
static inline size_t
bare_type_stripper__scan_declare(const utf8_t *s, size_t n, size_t i) {
  // Balance '()'/'[]'/'{}' uniformly, but only treat a closing '}' at outer
  // depth as the end of the declared body. This matters for 'declare function
  // f(): void' (no body): The ')' of params closes at depth 0 but isn't a
  // body, so we keep scanning past the return type until a ';' or newline.
  int depth = 0;

  while (i < n) {
    uint8_t cc = u(0);

    // A newline at depth 0 ends the construct, so check it before the
    // generic trivia skip (which would consume it).
    if (lt(cc) && depth == 0) break;

    if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

    if (cc == '{' || cc == '(' || cc == '[') {
      depth++;
      i++;
      continue;
    }
    if (cc == '}' || cc == ')' || cc == ']') {
      if (depth == 0) break;
      depth--;
      i++;
      if (depth == 0 && cc == '}') break;
      continue;
    }

    if (cc == ';' && depth == 0) {
      i++;
      break;
    }

    i++;
  }

  return i;
}

// Process 'declare ...' at statement position.
static inline int
bare_type_stripper__process_declare(bare_type_stripper_t *ctx, size_t *result) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t start = *result;
  size_t end = bare_type_stripper__scan_declare(s, n, start + 7);

  err = bare_type_stripper__add_range_flags(ctx, start, end, bare_type_stripper_semi);
  if (err < 0) return err;

  *result = end;
  return 0;
}

// Process the contents of an import or export named specifier list, the
// '{ ... }' body. Expects s[i] == '{'. Erases type-only specifiers ('type
// Foo', 'type Foo as Bar') entirely, including the separating comma so the
// remaining value specifiers stay syntactically valid. Leaves cursor just
// past the matching '}'.
//
// `allow_star` is true for import lists - which can be parsed by callers
// that also want to handle 'import * as X from "..."' (the '*' itself is at
// outer scope, but a defensive 'type *' check stays consistent there) - and
// false for export lists where '*' isn't allowed inside the braces.
static inline int
bare_type_stripper__process_named_specifiers(bare_type_stripper_t *ctx, size_t *result, bool allow_star) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  if (i >= n || u(0) != '{') return 0;
  i++;

  // Position just after the last kept (non-type) specifier's content, before
  // its trailing comma if any. Used to absorb the preceding comma when the
  // final specifier in the list is type-only.
  size_t prev_kept_end = i;

  while (i < n) {
    i = bare_type_stripper__skip_trivia(s, n, i);
    if (i >= n) break;
    if (u(0) == '}') break;

    size_t spec_start = i;
    bool is_type = false;

    if (bare_type_stripper__at_kw(s, n, i, "type", 4)) {
      size_t after = bare_type_stripper__skip_trivia(s, n, i + 4);
      if (after < n && (ids(s[after]) || (allow_star && s[after] == '*'))) {
        is_type = true;
        i = after;
      }
    }

    // Specifier name is either an identifier, '*' (import only), or a string
    // literal (arbitrary module namespace names, e.g. 'import { "x" as C }').
    if (i < n && (u(0) == '\'' || u(0) == '"')) {
      i = bare_type_stripper__skip_string(s, n, i);
    } else if (allow_star && i < n && u(0) == '*') {
      i++;
    } else {
      while (i < n && id(u(0))) i++;
    }
    size_t spec_end = i;

    i = bare_type_stripper__skip_trivia(s, n, i);
    if (bare_type_stripper__at_kw(s, n, i, "as", 2)) {
      i += 2;
      i = bare_type_stripper__skip_trivia(s, n, i);
      if (i < n && (u(0) == '\'' || u(0) == '"')) {
        i = bare_type_stripper__skip_string(s, n, i);
      } else {
        while (i < n && id(u(0))) i++;
      }
      spec_end = i;
      i = bare_type_stripper__skip_trivia(s, n, i);
    }

    bool has_comma = false;
    size_t after_comma = spec_end;
    if (i < n && u(0) == ',') {
      has_comma = true;
      i++;
      after_comma = i;
    }

    if (is_type) {
      size_t strip_start = has_comma ? spec_start : prev_kept_end;

      err = bare_type_stripper__add_range(ctx, strip_start, after_comma);
      if (err < 0) return err;
    } else {
      prev_kept_end = spec_end;
    }
  }

  if (i < n && u(0) == '}') i++;

  *result = i;
  return 0;
}

// Process 'import' at statement position. Handles 'import type ...' and
// 'import { type X, ... }'. Leaves dynamic import and import.meta alone.
static inline int
bare_type_stripper__process_import(bare_type_stripper_t *ctx, size_t *result) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;
  size_t start = i;
  i += 6;

  size_t after_kw = bare_type_stripper__skip_trivia(s, n, i);
  if (after_kw < n && (s[after_kw] == '(' || s[after_kw] == '.')) {
    *result = i;
    return 0;
  }
  i = after_kw;

  // 'import type ...' - erase whole statement.
  if (bare_type_stripper__at_kw(s, n, i, "type", 4)) {
    i += 4;

    while (i < n) {
      uint8_t cc = u(0);
      if (cc == '\'' || cc == '"') {
        i = bare_type_stripper__skip_string(s, n, i);
        size_t peek = bare_type_stripper__skip_trivia(s, n, i);
        if (peek < n && s[peek] == ';') i = peek + 1;
        break;
      }
      if (cc == '/' && i + 1 < n && (u(1) == '/' || u(1) == '*')) {
        i = bare_type_stripper__skip_trivia(s, n, i);
        continue;
      }
      if (cc == ';') {
        i++;
        break;
      }
      i++;
    }

    err = bare_type_stripper__add_range_flags(ctx, start, i, bare_type_stripper_semi);
    if (err < 0) return err;

    *result = i;
    return 0;
  }

  // Walk the import clause looking for '{...}' named list. Type-only
  // specifiers ('type Foo', 'type Foo as Bar') are erased entirely along
  // with their separating comma, since the named binding would otherwise be
  // requested as a value at runtime and fail.
  while (i < n) {
    uint8_t cc = u(0);
    if (ws(cc)) {
      i++;
      continue;
    }
    if (cc == '/' && i + 1 < n && (u(1) == '/' || u(1) == '*')) {
      i = bare_type_stripper__skip_trivia(s, n, i);
      continue;
    }
    if (cc == '\'' || cc == '"') {
      i = bare_type_stripper__skip_string(s, n, i);
      break;
    }

    if (cc == '{') {
      err = bare_type_stripper__process_named_specifiers(ctx, &i, true);
      if (err < 0) return err;
      continue;
    }

    if (cc == ';') {
      i++;
      break;
    }

    i++;
  }

  *result = i;
  return 0;
}

// Process 'export' at statement position.
static inline int
bare_type_stripper__process_export(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;
  size_t start = i;
  i += 6;

  i = bare_type_stripper__skip_trivia(s, n, i);
  if (i >= n) {
    *result = i;
    return 0;
  }

  // 'export type ...' - either an exported alias ('export type Foo = ...')
  // or a type-only re-export ('export type { ... } from ...', 'export type
  // * as NS from ...'). Both are erased whole, including 'export'.
  if (bare_type_stripper__at_kw(s, n, i, "type", 4)) {
    size_t after = bare_type_stripper__skip_trivia(s, n, i + 4);

    // Alias form - delegate to the type-alias scanner so multi-line types
    // behave exactly like a plain 'type' declaration.
    if (after < n && ids(s[after])) {
      size_t end = bare_type_stripper__scan_type_alias(s, n, i + 4);

      err = bare_type_stripper__add_range_flags(ctx, start, end, bare_type_stripper_semi);
      if (err < 0) return err;

      *result = end;
      return 0;
    }

    // Re-export form - scan the specifier clause, the optional 'from'
    // clause, and the terminating ';'.
    if (after < n && (s[after] == '{' || s[after] == '*')) {
      i = after;

      if (u(0) == '{') {
        i = bare_type_stripper__scan_balanced(s, n, i, '{', '}');
      } else {
        // '* [as NS]'.
        i++;

        size_t p = bare_type_stripper__skip_trivia(s, n, i);
        if (bare_type_stripper__at_kw(s, n, p, "as", 2)) {
          p = bare_type_stripper__skip_trivia(s, n, p + 2);
          while (p < n && id(s[p])) p++;
          i = p;
        }
      }

      size_t p = bare_type_stripper__skip_trivia(s, n, i);
      if (bare_type_stripper__at_kw(s, n, p, "from", 4)) {
        p = bare_type_stripper__skip_trivia(s, n, p + 4);
        if (p < n && (s[p] == '\'' || s[p] == '"')) {
          i = bare_type_stripper__skip_string(s, n, p);
        }
      }

      size_t semi = bare_type_stripper__skip_trivia(s, n, i);
      if (semi < n && s[semi] == ';') i = semi + 1;

      err = bare_type_stripper__add_range_flags(ctx, start, i, bare_type_stripper_semi);
      if (err < 0) return err;

      *result = i;
      return 0;
    }
  }

  // 'export declare ...' - the whole statement, including 'export', is
  // ambient and erased as one range; stripping only from 'declare' onward
  // would leave an invalid bare 'export ;' behind.
  if (bare_type_stripper__at_kw(s, n, i, "declare", 7)) {
    size_t after = bare_type_stripper__skip_trivia(s, n, i + 7);
    if (after < n && ids(s[after])) {
      size_t end = bare_type_stripper__scan_declare(s, n, i + 7);

      err = bare_type_stripper__add_range_flags(ctx, start, end, bare_type_stripper_semi);
      if (err < 0) return err;

      *result = end;
      return 0;
    }
  }

  // 'export interface ...' - erase as one range covering 'export ' too.
  if (bare_type_stripper__at_kw(s, n, i, "interface", 9)) {
    size_t end = bare_type_stripper__scan_interface(s, n, i);

    err = bare_type_stripper__add_range_flags(ctx, start, end, bare_type_stripper_semi);
    if (err < 0) return err;

    *result = end;
    return 0;
  }

  // 'export [default] function ...' - delegate, threading through the
  // statement start so a bodyless overload signature erases the 'export'
  // prefix too instead of leaving a dangling 'export ;' behind.
  size_t p = i;

  if (bare_type_stripper__at_kw(s, n, p, "default", 7)) {
    p = bare_type_stripper__skip_trivia(s, n, p + 7);
  }

  if (bare_type_stripper__at_kw(s, n, p, "function", 8)) {
    err = bare_type_stripper__process_function(ctx, &p, start, depth);
    if (err < 0) return err;

    *result = p;
    return 0;
  }

  // 'export { ... }' - erase type-only specifiers entirely, including the
  // comma that separates them from value specifiers.
  if (u(0) == '{') {
    err = bare_type_stripper__process_named_specifiers(ctx, &i, false);
    if (err < 0) return err;
  }

  *result = i;
  return 0;
}

// Process a variable declaration sequence (the right-hand side of const/let/var).
// On entry, i points just past the keyword.
static inline int
bare_type_stripper__process_var_decl(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  while (i < n) {
    i = bare_type_stripper__skip_trivia(s, n, i);
    if (i >= n) break;

    // Destructuring pattern or identifier.
    if (u(0) == '{' || u(0) == '[') {
      uint8_t open = u(0);
      uint8_t close = open == '{' ? '}' : ']';
      i = bare_type_stripper__scan_balanced(s, n, i, open, close);
    } else if (ids(u(0))) {
      while (i < n && id(u(0))) i++;
    } else {
      break;
    }

    i = bare_type_stripper__skip_trivia(s, n, i);

    // Definite assignment '!:'.
    if (i < n && u(0) == '!') {
      size_t peek = bare_type_stripper__skip_trivia(s, n, i + 1);
      if (peek < n && s[peek] == ':') {
        err = bare_type_stripper__add_range(ctx, i, i + 1);
        if (err < 0) return err;

        i++;
        i = bare_type_stripper__skip_trivia(s, n, i);
      }
    }

    // Type annotation ': T'. T_INOF stops the scan at a bare 'in'/'of'
    // identifier - needed for 'for (const x: T of arr)' headers, harmless
    // elsewhere since neither word appears at depth 0 of a valid type.
    if (i < n && u(0) == ':') {
      size_t ts = i;
      i++;
      i = bare_type_stripper__scan_type(
        s,
        n,
        i,
        BARE_TYPE_STRIPPER__T_EQ | BARE_TYPE_STRIPPER__T_COMMA | BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_NL | BARE_TYPE_STRIPPER__T_INOF
      );

      err = bare_type_stripper__add_range(ctx, ts, i);
      if (err < 0) return err;

      i = bare_type_stripper__skip_trivia(s, n, i);
    }

    // Initializer. stop_at_asi keeps the walker from chewing past a newline
    // into the next statement when the declaration has no trailing ';',
    // which would otherwise miss type annotations on the following decl.
    if (i < n && u(0) == '=') {
      i++;

      err = bare_type_stripper__walk(ctx, &i, ';', false, true, depth + 1);
      if (err < 0) return err;
    }

    i = bare_type_stripper__skip_trivia(s, n, i);
    if (i < n && u(0) == ',') {
      i++;
      continue;
    }
    break;
  }

  *result = i;
  return 0;
}

// Process 'function ...'. On entry, *result points at 'function' and
// stmt_start at the start of the enclosing statement - usually the same
// position, but earlier when the declaration is prefixed by
// 'export [default]'. A bodyless signature (an overload) is erased whole,
// starting from stmt_start so no dangling 'export' survives.
static inline int
bare_type_stripper__process_function(bare_type_stripper_t *ctx, size_t *result, size_t stmt_start, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  uint32_t mark = ctx->len;
  size_t i = *result;
  i += 8;

  i = bare_type_stripper__skip_trivia(s, n, i);

  // 'function' used as a property key ('{ function: 1 }') rather than a
  // declaration or expression - leave it alone. A real function never has
  // ':' directly after the keyword.
  if (i < n && u(0) == ':') {
    *result += 8;
    return 0;
  }
  if (i < n && u(0) == '*') {
    i++;
    i = bare_type_stripper__skip_trivia(s, n, i);
  }

  i = bare_type_stripper__skip_name(s, n, i);

  err = bare_type_stripper__strip_generic(ctx, &i);
  if (err < 0) return err;

  i = bare_type_stripper__skip_trivia(s, n, i);

  if (i < n && u(0) == '(') {
    err = bare_type_stripper__process_params(ctx, &i, depth);
    if (err < 0) return err;
  }

  i = bare_type_stripper__skip_trivia(s, n, i);

  if (i < n && u(0) == ':') {
    size_t ts = i;
    i++;
    i = bare_type_stripper__scan_type(
      s,
      n,
      i,
      BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_NL
    );

    err = bare_type_stripper__add_range(ctx, ts, i);
    if (err < 0) return err;

    i = bare_type_stripper__skip_trivia(s, n, i);
  }

  if (i < n && u(0) == '{') {
    err = bare_type_stripper__walk(ctx, &i, '}', true, false, depth + 1);
    if (err < 0) return err;
  } else {
    // Overload signature: A function declaration without a body, terminated
    // by ';' or a line break. Drop the ranges emitted for its pieces and
    // erase the whole signature as one range instead - leaving a bodyless
    // 'function f()' behind would produce invalid runtime JS. The injected
    // ';' keeps ASI from folding the signature line into its neighbors.
    size_t peek = bare_type_stripper__skip_trivia(s, n, i);
    if (peek < n && s[peek] == ';') i = peek + 1;

    ctx->len = mark;

    err = bare_type_stripper__add_range_flags(ctx, stmt_start, i, bare_type_stripper_semi);
    if (err < 0) return err;
  }

  *result = i;
  return 0;
}

// Process 'class ...'. On entry, i points at 'class'.
static inline int
bare_type_stripper__process_class(bare_type_stripper_t *ctx, size_t *result, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  i = bare_type_stripper__skip_name(s, n, i + 5);

  err = bare_type_stripper__strip_generic(ctx, &i);
  if (err < 0) return err;

  i = bare_type_stripper__skip_trivia(s, n, i);

  // 'extends X[<...>]' - keep but strip generic arguments and 'implements'.
  for (;;) {
    if (bare_type_stripper__at_kw(s, n, i, "extends", 7)) {
      i += 7;
      i = bare_type_stripper__skip_trivia(s, n, i);

      // Skip the expression to the next significant boundary, but strip any
      // '<...>' generic arguments that appear at top level.
      int depth = 0;

      while (i < n) {
        if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

        uint8_t cc = u(0);

        if (depth == 0 && cc == '<') {
          size_t before = i;

          err = bare_type_stripper__strip_generic(ctx, &i);
          if (err < 0) return err;

          if (i == before) i++;
          continue;
        }

        if (cc == '(' || cc == '[' || cc == '{') {
          if (depth == 0 && cc == '{') break;
          depth++;
          i++;
          continue;
        }
        if (cc == ')' || cc == ']' || cc == '}') {
          if (depth == 0) break;
          depth--;
          i++;
          continue;
        }

        if (depth == 0 && cc == ',') {
          i++;
          break;
        }
        if (depth == 0 && ids(cc)) {
          // Could be 'implements' keyword.
          if (bare_type_stripper__at_kw(s, n, i, "implements", 10)) break;
          while (i < n && id(u(0))) i++;
          continue;
        }

        i++;
      }

      i = bare_type_stripper__skip_trivia(s, n, i);
      continue;
    }

    if (bare_type_stripper__at_kw(s, n, i, "implements", 10)) {
      size_t ims = i;
      i += 10;
      int depth = 0;

      while (i < n) {
        if (bare_type_stripper__try_skip_inert(s, n, &i)) continue;

        uint8_t cc = u(0);

        if (cc == '<' || cc == '(' || cc == '[') {
          depth++;
          i++;
          continue;
        }
        if (cc == '>' || cc == ')' || cc == ']') {
          if (depth == 0) break;
          depth--;
          i++;
          continue;
        }
        if (cc == '{' && depth == 0) break;

        i++;
      }

      err = bare_type_stripper__add_range(ctx, ims, i);
      if (err < 0) return err;

      i = bare_type_stripper__skip_trivia(s, n, i);
      continue;
    }

    break;
  }

  if (i < n && u(0) == '{') {
    err = bare_type_stripper__process_class_body(ctx, &i, depth);
    if (err < 0) return err;
  }

  *result = i;
  return 0;
}

// Keyword classification for the walker: Maps a keyword to the `prev` class
// it leaves behind. Keywords with side effects (declaration forms, 'as' /
// 'satisfies', 'import' / 'export', ...) are dispatched explicitly in the
// walker before this table is consulted.
typedef struct {
  const char *kw;
  uint8_t len;
  uint8_t prev;
} bare_type_stripper__kw_t;

static const bare_type_stripper__kw_t bare_type_stripper__kws[] = {
  // Expression-starting keywords: The next token begins a new expression.
  {"in", 2, bare_type_stripper__prev_op},
  {"new", 3, bare_type_stripper__prev_op},
  {"void", 4, bare_type_stripper__prev_op},
  {"case", 4, bare_type_stripper__prev_op},
  {"throw", 5, bare_type_stripper__prev_op},
  {"await", 5, bare_type_stripper__prev_op},
  {"yield", 5, bare_type_stripper__prev_op},
  {"return", 6, bare_type_stripper__prev_op},
  {"typeof", 6, bare_type_stripper__prev_op},
  {"delete", 6, bare_type_stripper__prev_op},
  {"instanceof", 10, bare_type_stripper__prev_op},

  // Literal-like keywords: End an expression.
  {"true", 4, bare_type_stripper__prev_expr},
  {"this", 4, bare_type_stripper__prev_expr},
  {"null", 4, bare_type_stripper__prev_expr},
  {"false", 5, bare_type_stripper__prev_expr},
  {"super", 5, bare_type_stripper__prev_expr},
  {"arguments", 9, bare_type_stripper__prev_expr},
  {"undefined", 9, bare_type_stripper__prev_expr},

  // Statement keywords whose body follows directly: A '{' after these is a
  // block, not an object literal, and the next token sits at statement
  // position.
  {"do", 2, bare_type_stripper__prev_stmt},
  {"try", 3, bare_type_stripper__prev_stmt},
  {"else", 4, bare_type_stripper__prev_stmt},
  {"finally", 7, bare_type_stripper__prev_stmt},

  // 'async' is remembered so a following '(' can still be recognized as
  // arrow parameters.
  {"async", 5, bare_type_stripper__prev_async},
};

// Classify an identifier span against the keyword table. Returns the `prev`
// class the keyword leaves behind, or prev_expr for plain identifiers.
static inline int
bare_type_stripper__classify_kw(const utf8_t *s, size_t ks, size_t kl) {
  // Every entry is 2-10 bytes long; gate on length and first byte before
  // paying for a memcmp, since plain identifiers vastly outnumber keywords.
  if (kl < 2 || kl > 10) return bare_type_stripper__prev_expr;

  uint8_t c0 = s[ks];

  for (size_t k = 0; k < sizeof(bare_type_stripper__kws) / sizeof(bare_type_stripper__kws[0]); k++) {
    const bare_type_stripper__kw_t *kw = &bare_type_stripper__kws[k];

    if (kw->len == kl && (uint8_t) kw->kw[0] == c0 && memcmp(&s[ks], kw->kw, kl) == 0) {
      return kw->prev;
    }
  }

  return bare_type_stripper__prev_expr;
}

// Maximum walker recursion depth. The walker can recurse on '{' blocks and
// (via the process_* helpers) on function/class bodies; capping the depth
// keeps deeply nested input from blowing the C stack. 256 is generous in
// practice - real code rarely nests more than a few dozen levels.
#define BARE_TYPE_STRIPPER__MAX_DEPTH 256

// The master walker. Walks bytes from *result and either:
//  * If `stop_at` is non-zero, stops at the first occurrence of that byte at
//    depth 0 (relative to our local paren/bracket counters), without
//    consuming it. Two modes stop at more than one byte: ';' also stops at
//    ',' (variable declarators), and ',' also stops at ')' (default
//    parameter values).
//  * If `stop_at` is '}', expects to be called with i pointing AT the opening
//    '{' or after it - handled by the `body_is_stmt` flag: When true, this
//    walker recursively treats nested '{' blocks as statement contexts.
//
// `body_is_stmt` tells us whether the current block context behaves like a
// statement region (so 'type', 'interface', etc. should be matched as
// statement-leading keywords).
static inline int
bare_type_stripper__walk(bare_type_stripper_t *ctx, size_t *result, uint8_t stop_at, bool body_is_stmt, bool stop_at_asi, uint32_t depth) {
  const utf8_t *s = ctx->s;
  size_t n = ctx->n;

  int err;
  size_t i = *result;

  // Bail out if we've recursed too deeply. We jump to end-of-input so the
  // enclosing scan finishes cleanly rather than getting stuck in a partial
  // state; the (very deeply nested) tail won't be stripped, which is no
  // worse than the alternative of crashing.
  if (depth > BARE_TYPE_STRIPPER__MAX_DEPTH) {
    *result = n;
    return 0;
  }

  // If we're entering a block scope, consume the opening '{'.
  if (stop_at == '}' && i < n && u(0) == '{') i++;

  int paren_depth = 0;
  int bracket_depth = 0;

  // Unclosed ternary '?' count at this walk level. Used to keep the arrow
  // return-type lookahead from claiming the ':' of an enclosing ternary
  // ('c ? (a) : b => c' is a ternary, not an annotated arrow).
  int ternary_depth = 0;

  // Previous-token classification - see enum at top.
  int prev = body_is_stmt ? bare_type_stripper__prev_none : bare_type_stripper__prev_op;
  bool prev_dot = false;

  while (i < n) {
    uint8_t ch = u(0);

    // Whitespace.
    if (ws(ch)) {
      // ASI-aware stop: A newline at depth 0 right after an expression-
      // ending token is the cheapest lexical approximation of where ASI
      // would insert. Callers ask for this when scanning a class field
      // initializer so we don't chew into the next class member.
      if (stop_at_asi && lt(ch) && paren_depth == 0 && bracket_depth == 0 && bare_type_stripper__prev_is_expr(prev)) {
        *result = i;
        return 0;
      }
      // In statement contexts, treat a newline at depth 0 following an
      // expression-ending token as an ASI boundary - i.e. the next
      // identifier could be a statement-leading keyword ('type', 'declare',
      // 'interface', 'import', 'export'). Don't reset inside expression
      // contexts like object literals.
      if (body_is_stmt && lt(ch) && paren_depth == 0 && bracket_depth == 0 && bare_type_stripper__prev_is_expr(prev)) {
        prev = bare_type_stripper__prev_stmt;
      }
      i++;
      continue;
    }

    // Comments.
    if (ch == '/' && i + 1 < n && u(1) == '/') {
      while (i < n && !lt(u(0))) i++;
      continue;
    }
    if (ch == '/' && i + 1 < n && u(1) == '*') {
      i += 2;
      while (i + 1 < n && !(u(0) == '*' && u(1) == '/')) i++;
      if (i + 1 < n) i += 2;
      continue;
    }

    // Stop condition.
    if (stop_at != 0 && paren_depth == 0 && bracket_depth == 0) {
      if (stop_at == '}' && ch == '}') {
        i++;
        *result = i;
        return 0;
      }
      if (stop_at == ')' && ch == ')') {
        *result = i;
        return 0;
      }
      if (stop_at == ';' && (ch == ';' || ch == ',')) {
        *result = i;
        return 0;
      }
      if (stop_at == ',' && (ch == ',' || ch == ')' || ch == '}')) {
        *result = i;
        return 0;
      }
    }

    // A token follows. Latch and clear the pending property-access marker -
    // only the '.' / '?.' branches re-arm it.
    bool was_dot = prev_dot;
    prev_dot = false;

    // String, template, regex.
    if (ch == '\'' || ch == '"') {
      i = bare_type_stripper__skip_string(s, n, i);
      prev = bare_type_stripper__prev_expr;
      continue;
    }
    if (ch == '`') {
      err = bare_type_stripper__walk_template(ctx, &i, depth);
      if (err < 0) return err;

      prev = bare_type_stripper__prev_expr;
      continue;
    }
    if (ch == '/') {
      if (!bare_type_stripper__prev_is_expr(prev)) {
        i = bare_type_stripper__skip_regex(s, n, i);
        prev = bare_type_stripper__prev_expr;
        continue;
      }
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    // Numeric literal.
    if (ch >= '0' && ch <= '9') {
      while (i < n && (id(u(0)) || u(0) == '.')) i++;
      prev = bare_type_stripper__prev_expr;
      continue;
    }

    // Identifier or keyword.
    if (ids(ch)) {
      size_t ks = i;
      while (i < n && id(u(0))) i++;
      size_t ke = i;
      size_t kl = ke - ks;

      // Property access: This identifier is just a property name.
      if (was_dot) {
        prev = bare_type_stripper__prev_expr;
        continue;
      }

      bool at_stmt_start = (prev == bare_type_stripper__prev_stmt || prev == bare_type_stripper__prev_none);

      // Statement-leading TS keywords. The three declaration forms ('type',
      // 'interface', 'declare') share an identical dispatch pattern: When
      // followed by an identifier, delegate to a process_* helper. 'abstract'
      // is a special case - it only erases the keyword and lets the normal
      // 'class' branch handle the rest.
      if (at_stmt_start) {
        int (*handler)(bare_type_stripper_t *, size_t *) = NULL;

        if (bare_type_stripper__match_kw(s, ks, kl, "type", 4)) handler = bare_type_stripper__process_type_alias;
        else if (bare_type_stripper__match_kw(s, ks, kl, "interface", 9)) handler = bare_type_stripper__process_interface;
        else if (bare_type_stripper__match_kw(s, ks, kl, "declare", 7)) handler = bare_type_stripper__process_declare;

        if (handler) {
          size_t after = bare_type_stripper__skip_trivia(s, n, ke);
          if (after < n && ids(s[after])) {
            size_t save = ks;

            err = handler(ctx, &save);
            if (err < 0) return err;

            i = save;
            prev = bare_type_stripper__prev_stmt;
            continue;
          }
        }

        // 'abstract class' - strip the leading 'abstract' keyword. The 'class'
        // that follows is processed by the normal class branch below.
        if (bare_type_stripper__match_kw(s, ks, kl, "abstract", 8)) {
          size_t after = bare_type_stripper__skip_trivia(s, n, ke);
          if (bare_type_stripper__at_kw(s, n, after, "class", 5)) {
            err = bare_type_stripper__add_range(ctx, ks, ke);
            if (err < 0) return err;

            i = ke;
            continue;
          }
        }

        // 'enum E { ... }' - non-erasable: An enum declaration emits a
        // runtime object that stripping can't reproduce. Mark the whole
        // construct so strip() can throw while lex() consumers still see
        // its extent. ('declare enum' is ambient and already handled above.)
        if (bare_type_stripper__match_kw(s, ks, kl, "enum", 4)) {
          size_t after = bare_type_stripper__skip_trivia(s, n, ke);
          if (after < n && ids(s[after])) {
            size_t end = bare_type_stripper__scan_enum(s, n, ks);

            err = bare_type_stripper__add_range_flags(ctx, ks, end, bare_type_stripper_error);
            if (err < 0) return err;

            i = end;
            prev = bare_type_stripper__prev_stmt;
            continue;
          }
        }

        // 'namespace N { ... }' / 'module N { ... }' - non-erasable for the
        // same reason. Requires a '{' body after the (possibly dotted) name
        // so identifiers that merely spell the keywords ('module.exports')
        // are left alone.
        if (bare_type_stripper__match_kw(s, ks, kl, "namespace", 9) || bare_type_stripper__match_kw(s, ks, kl, "module", 6)) {
          size_t p = bare_type_stripper__skip_trivia(s, n, ke);
          if (p < n && ids(s[p])) {
            while (p < n && (id(s[p]) || s[p] == '.')) p++;
            p = bare_type_stripper__skip_trivia(s, n, p);

            if (p < n && s[p] == '{') {
              size_t end = bare_type_stripper__scan_balanced(s, n, p, '{', '}');

              err = bare_type_stripper__add_range_flags(ctx, ks, end, bare_type_stripper_error);
              if (err < 0) return err;

              i = end;
              prev = bare_type_stripper__prev_stmt;
              continue;
            }
          }
        }
      }

      // 'catch (e: T)' - strip the TS type annotation inside the catch
      // parameter list. 'catch' is a reserved word so its only valid context
      // is the catch clause of a try-statement; we don't gate on prev. The
      // outer walker still processes 'catch (e) { ... }' as usual.
      if (kl == 5 && memcmp(&s[ks], "catch", 5) == 0) {
        size_t after = bare_type_stripper__skip_trivia(s, n, ke);
        if (after < n && s[after] == '(') {
          size_t p = bare_type_stripper__skip_trivia(s, n, after + 1);
          if (p < n && ids(s[p])) {
            while (p < n && id(s[p])) p++;
            size_t scan = bare_type_stripper__skip_trivia(s, n, p);
            if (scan < n && s[scan] == ':') {
              size_t ts = scan;
              size_t te = bare_type_stripper__scan_type(s, n, scan + 1, BARE_TYPE_STRIPPER__T_PAREN);

              err = bare_type_stripper__add_range(ctx, ts, te);
              if (err < 0) return err;
            }
          }
        }
      }

      // 'import' and 'export' - module-level statement keywords. Allow these
      // even when prev is something other than stmt_start, since they're
      // reserved words and our prev tracking can be imprecise at module level.
      if (kl == 6 && memcmp(&s[ks], "import", 6) == 0) {
        // Dynamic import or import.meta - leave as expression.
        size_t after = bare_type_stripper__skip_trivia(s, n, ke);
        if (after < n && (s[after] == '(' || s[after] == '.')) {
          i = ke;
          prev = bare_type_stripper__prev_expr;
          continue;
        }

        if (at_stmt_start) {
          size_t save = ks;

          err = bare_type_stripper__process_import(ctx, &save);
          if (err < 0) return err;

          i = save;
          prev = bare_type_stripper__prev_stmt;
          continue;
        }
      }

      if (kl == 6 && memcmp(&s[ks], "export", 6) == 0 && at_stmt_start) {
        size_t save = ks;

        err = bare_type_stripper__process_export(ctx, &save, depth);
        if (err < 0) return err;

        i = save;
        prev = bare_type_stripper__prev_stmt;
        continue;
      }

      // 'function' / 'class' - declaration or expression.
      if (kl == 8 && memcmp(&s[ks], "function", 8) == 0) {
        size_t save = ks;

        err = bare_type_stripper__process_function(ctx, &save, ks, depth);
        if (err < 0) return err;

        i = save;
        prev = at_stmt_start ? bare_type_stripper__prev_stmt : bare_type_stripper__prev_expr;
        continue;
      }

      if (kl == 5 && memcmp(&s[ks], "class", 5) == 0) {
        size_t save = ks;

        err = bare_type_stripper__process_class(ctx, &save, depth);
        if (err < 0) return err;

        i = save;
        prev = at_stmt_start ? bare_type_stripper__prev_stmt : bare_type_stripper__prev_expr;
        continue;
      }

      // Variable declarations - const/let/var. These appear at statement
      // starts and in for-loop headers ('for (const x = ...)'); we accept
      // any prev that isn't an expression-ending position.
      if (
        !bare_type_stripper__prev_is_expr(prev) &&
        ((kl == 3 && (memcmp(&s[ks], "let", 3) == 0 ||
                      memcmp(&s[ks], "var", 3) == 0)) ||
         (kl == 5 && memcmp(&s[ks], "const", 5) == 0))
      ) {
        // 'const enum E { ... }' is an enum declaration, not a variable -
        // non-erasable like a plain 'enum'.
        if (kl == 5) {
          size_t after = bare_type_stripper__skip_trivia(s, n, ke);
          if (bare_type_stripper__at_kw(s, n, after, "enum", 4)) {
            size_t name = bare_type_stripper__skip_trivia(s, n, after + 4);
            if (name < n && ids(s[name])) {
              size_t end = bare_type_stripper__scan_enum(s, n, after);

              err = bare_type_stripper__add_range_flags(ctx, ks, end, bare_type_stripper_error);
              if (err < 0) return err;

              i = end;
              prev = bare_type_stripper__prev_stmt;
              continue;
            }
          }
        }

        i = ke;

        err = bare_type_stripper__process_var_decl(ctx, &i, depth);
        if (err < 0) return err;

        prev = bare_type_stripper__prev_stmt;
        continue;
      }

      // Postfix type assertion keywords 'as' and 'satisfies' - both consume a
      // following type expression with the same terminator set.
      if (bare_type_stripper__prev_is_expr(prev) && (bare_type_stripper__match_kw(s, ks, kl, "as", 2) || bare_type_stripper__match_kw(s, ks, kl, "satisfies", 9))) {
        size_t te = bare_type_stripper__scan_type(
          s,
          n,
          ke,
          BARE_TYPE_STRIPPER__T_COMMA | BARE_TYPE_STRIPPER__T_SEMI | BARE_TYPE_STRIPPER__T_PAREN | BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_BRACK | BARE_TYPE_STRIPPER__T_EQ | BARE_TYPE_STRIPPER__T_NL
        );

        uint32_t flags = bare_type_stripper__asi_hazard_after(s, n, te) ? bare_type_stripper_semi : 0;

        err = bare_type_stripper__add_range_flags(ctx, ks, te, flags);
        if (err < 0) return err;

        i = te;
        prev = bare_type_stripper__prev_expr;
        continue;
      }

      // Remaining keywords only affect token classification - see the
      // keyword table above the walker. Plain identifiers end an expression.
      prev = bare_type_stripper__classify_kw(s, ks, kl);
      continue;
    }

    // Punctuation.
    switch (ch) {
    case '.': {
      // Spread / rest '...'
      if (i + 2 < n && u(1) == '.' && u(2) == '.') {
        i += 3;
        prev = bare_type_stripper__prev_op;
        continue;
      }
      // Otherwise '.' is a member operator; 'prev' is unchanged.
      i++;
      prev_dot = true;
      continue;
    }

    case '?': {
      // Optional chaining '?.'
      if (i + 1 < n && u(1) == '.') {
        i += 2;
        prev_dot = true;
        continue;
      }
      // Nullish coalescing '??' (optionally '??=').
      if (i + 1 < n && u(1) == '?') {
        i += 2;
        if (i < n && u(0) == '=') i++;
        prev = bare_type_stripper__prev_op;
        continue;
      }
      // Ternary '?'.
      ternary_depth++;
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case '(': {
      // Detect arrow function parameter lists: '(...)' immediately followed
      // by '=>' or by ': TYPE =>'. Without this we miss type annotations
      // inside arrow params. A '(' after an expression is a call and can't
      // start arrow parameters, so the lookahead is skipped entirely there -
      // except after the identifier 'async', which prefixes arrow parameters
      // while otherwise lexing like a plain expression.
      bool maybe_arrow = !bare_type_stripper__prev_is_expr(prev) || prev == bare_type_stripper__prev_async;

      bool is_arrow = false;
      if (maybe_arrow) {
        size_t pend = bare_type_stripper__scan_group(ctx, i);
        size_t after = bare_type_stripper__skip_trivia(s, n, pend);

        if (after + 1 < n && s[after] == '=' && s[after + 1] == '>') {
          is_arrow = true;
        } else if (after < n && s[after] == ':' && ternary_depth == 0) {
          // With an open ternary, a ':' after ')' is the ternary's alternate
          // separator ('c ? (a) : b => c'), not a return-type annotation.
          // Arrow return types can legitimately span newlines, so we don't
          // pass T_NL here - only the unambiguous terminators.
          size_t type_end = bare_type_stripper__scan_type(
            s,
            n,
            after + 1,
            BARE_TYPE_STRIPPER__T_ARROW | BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_SEMI
          );

          size_t after_type = bare_type_stripper__skip_trivia(s, n, type_end);

          if (after_type + 1 < n && s[after_type] == '=' && s[after_type + 1] == '>') {
            is_arrow = true;
          }
        }
      }

      if (is_arrow) {
        err = bare_type_stripper__process_params(ctx, &i, depth);
        if (err < 0) return err;

        // The original ')' sits one byte before i (process_params consumed it).
        size_t orig_paren = i - 1;

        size_t aft = bare_type_stripper__skip_trivia(s, n, i);
        if (aft < n && s[aft] == ':') {
          size_t ts = aft;
          i = aft + 1;
          i = bare_type_stripper__scan_type(
            s,
            n,
            i,
            BARE_TYPE_STRIPPER__T_ARROW | BARE_TYPE_STRIPPER__T_BRACE | BARE_TYPE_STRIPPER__T_SEMI
          );

          // If anything between the params ')' and '=>' spans a newline -
          // whether inside the return-type itself OR in the gap between ')'
          // and ':' - JS would see a LineTerminator before '=>' after the
          // type is blanked, which is a syntax error. Move the ')' down:
          // Strip the original one and place a replacement ')' at the last
          // non-whitespace position before '=>'.
          bool multiline = false;
          for (size_t j = orig_paren + 1; j < i; j++) {
            if (lt(s[j])) {
              multiline = true;
              break;
            }
          }

          if (multiline) {
            // Walk back past whitespace to find a byte to repurpose as ')'.
            size_t target = i;
            while (target > orig_paren + 1 && ws(s[target - 1])) target--;

            // For the relocation to help, the target byte must live on the
            // same line as '=>' (no newlines between them) - otherwise we'd
            // just put ')' on a different orphaned line, still leaving a
            // line terminator before '=>'. When that's the case we fall
            // back to the plain strip; the output won't be a valid arrow
            // but it's no worse than the naive strip.
            bool same_line = true;
            for (size_t j = target; j < i; j++) {
              if (lt(s[j])) {
                same_line = false;
                break;
              }
            }

            if (target > orig_paren + 1 && same_line) {
              err = bare_type_stripper__add_range(ctx, orig_paren, i);
              if (err < 0) return err;

              err = bare_type_stripper__add_range_flags(ctx, target - 1, target, bare_type_stripper_paren);
              if (err < 0) return err;
            } else {
              err = bare_type_stripper__add_range(ctx, ts, i);
              if (err < 0) return err;
            }
          } else {
            err = bare_type_stripper__add_range(ctx, ts, i);
            if (err < 0) return err;
          }
        }

        prev = bare_type_stripper__prev_op;
        continue;
      }

      paren_depth++;
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case ')': {
      if (paren_depth > 0) paren_depth--;
      i++;
      prev = bare_type_stripper__prev_paren;
      continue;
    }

    case '[': {
      bracket_depth++;
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case ']': {
      if (bracket_depth > 0) bracket_depth--;
      i++;
      prev = bare_type_stripper__prev_expr;
      continue;
    }

    case '{': {
      // Recurse. Decide whether this '{' starts a block (statement context)
      // or an object literal (expression context). A '{' right after ')'
      // (if/for/while/switch headers, function parameter lists) or after
      // '=>' (arrow function body) is always a block in valid code - an
      // object literal can't directly follow either token.
      bool is_arrow_body = prev == bare_type_stripper__prev_arrow;
      bool is_block_ctx =
        is_arrow_body ||
        prev == bare_type_stripper__prev_paren ||
        prev == bare_type_stripper__prev_stmt ||
        prev == bare_type_stripper__prev_none;

      size_t save = i;

      if (is_block_ctx) {
        err = bare_type_stripper__walk(ctx, &save, '}', true, false, depth + 1);
      } else {
        // Object literal - structured processing so shorthand method
        // signatures get their annotations stripped.
        err = bare_type_stripper__process_object_literal(ctx, &save, depth + 1);
      }
      if (err < 0) return err;

      i = save;
      // An arrow body ends the arrow expression; any other block ends a
      // statement.
      prev = is_block_ctx && !is_arrow_body
               ? bare_type_stripper__prev_stmt
               : bare_type_stripper__prev_expr;
      continue;
    }

    case '}': {
      // Unmatched '}' at our depth - treat as end of containing scope.
      if (stop_at == '}' && paren_depth == 0 && bracket_depth == 0) {
        i++;
        *result = i;
        return 0;
      }
      i++;
      prev = bare_type_stripper__prev_stmt;
      continue;
    }

    case ';': {
      ternary_depth = 0;
      i++;
      prev = bare_type_stripper__prev_stmt;
      continue;
    }

    case ',': {
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case '!': {
      // Postfix non-null assertion: Prev is an expression and not followed by '='.
      if (bare_type_stripper__prev_is_expr(prev) && (i + 1 >= n || u(1) != '=')) {
        err = bare_type_stripper__add_range(ctx, i, i + 1);
        if (err < 0) return err;

        i++;
        continue;
      }

      // '!' (logical not), '!=' or '!=='.
      i++;
      if (i < n && u(0) == '=') i++;
      if (i < n && u(0) == '=') i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case '<': {
      // Generic at call site or instantiation: Prev is an expression and a
      // balanced '<...>' is followed by a token that can't continue a JS
      // comparison expression. '(' / '`' mean a generic call or tagged
      // template. ')' / ',' / ';' / ']' / '}' / end-of-input mean a TS-only
      // generic instantiation expression (TS 4.7+) - 'foo<T>' returning a
      // typed reference - which would otherwise mis-parse as chained
      // comparisons in stripped JS.
      if (bare_type_stripper__prev_is_expr(prev)) {
        size_t ge = bare_type_stripper__scan_generic(s, n, i);
        if (ge > i + 1) {
          size_t peek = bare_type_stripper__skip_trivia(s, n, ge);
          bool is_generic = false;
          if (peek >= n) {
            is_generic = true;
          } else {
            uint8_t pc = s[peek];
            if (pc == '(' || pc == '`' || pc == ')' || pc == ',' || pc == ';' || pc == ']' || pc == '}') {
              is_generic = true;
            }
          }
          if (is_generic) {
            err = bare_type_stripper__add_range(ctx, i, ge);
            if (err < 0) return err;

            i = ge;
            continue;
          }
        }
      } else {
        // '<' at the start of an expression isn't valid JS - it's TS-only
        // syntax: Generic arrow type parameters ('<T,>(x) => x') when a '('
        // follows the balanced '<...>', otherwise an old-style angle cast
        // ('<T>expr') or JSX, which we can't tell apart. Strip the former;
        // mark the latter as non-erasable.
        size_t ge = bare_type_stripper__scan_generic(s, n, i);
        if (ge > i + 1) {
          size_t peek = bare_type_stripper__skip_trivia(s, n, ge);

          if (peek < n && s[peek] == '(') {
            err = bare_type_stripper__add_range(ctx, i, ge);
            if (err < 0) return err;
          } else {
            err = bare_type_stripper__add_range_flags(ctx, i, ge, bare_type_stripper_error);
            if (err < 0) return err;
          }

          i = ge;
          prev = bare_type_stripper__prev_op;
          continue;
        }
      }

      // '<', '<=', '<<' or '<<='.
      i++;
      if (i < n && u(0) == '=') i++;
      if (i < n && u(0) == '<') i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case '=': {
      i++;

      // Arrow '=>'.
      if (i < n && u(0) == '>') {
        i++;
        prev = bare_type_stripper__prev_arrow;
        continue;
      }

      // '==', '==='.
      if (i < n && u(0) == '=') i++;
      if (i < n && u(0) == '=') i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case ':': {
      if (ternary_depth > 0) ternary_depth--;
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }

    case '@': {
      // Decorator. The decorated thing (a class, class member or top-level
      // declaration) sits at a statement-leading position.
      err = bare_type_stripper__process_decorator(ctx, &i, depth);
      if (err < 0) return err;

      prev = bare_type_stripper__prev_stmt;
      continue;
    }

    default: {
      i++;
      prev = bare_type_stripper__prev_op;
      continue;
    }
    }
  }

  *result = i;
  return 0;
}

static inline int
bare_type_stripper__lex(bare_type_stripper_t *ctx, const utf8_t *s, size_t n) {
  size_t i = 0;

  ctx->s = s;
  ctx->n = n;

  ctx->ranges = NULL;
  ctx->len = 0;
  ctx->cap = 0;

  memset(ctx->paren_close, 0, sizeof(ctx->paren_close));

  return bare_type_stripper__walk(ctx, &i, 0, true, false, 0);
}

#undef u
#undef lt
#undef ws
#undef ids
#undef id

#endif // BARE_TYPE_STRIPPER_H
