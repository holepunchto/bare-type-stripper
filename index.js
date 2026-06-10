const binding = require('#binding')

module.exports = exports = function strip(input, encoding, opts = {}) {
  if (typeof encoding === 'object' && encoding !== null) {
    opts = encoding
    encoding = null
  }

  if (typeof input !== 'string' && !ArrayBuffer.isView(input)) {
    throw new TypeError(`Input must be a string or buffer. Received type ${typeof input}`)
  }

  const buffer = typeof input === 'string' ? Buffer.from(input, encoding) : input

  const { ranges } = binding.lex(buffer)

  check(buffer, ranges)

  return apply(buffer, ranges)
}

exports.lex = function lex(input, encoding, opts = {}) {
  if (typeof encoding === 'object' && encoding !== null) {
    opts = encoding
    encoding = null
  }

  if (typeof input !== 'string' && !ArrayBuffer.isView(input)) {
    throw new TypeError(`Input must be a string or buffer. Received type ${typeof input}`)
  }

  const { ranges } = binding.lex(typeof input === 'string' ? Buffer.from(input, encoding) : input)

  // The binding returns the ranges as a flat Uint32Array of (start, end,
  // flags) triples; reshape into the nested form.
  const result = []

  for (let r = 0; r < ranges.length; r += 3) {
    const flags = ranges[r + 2]

    result.push(flags ? [ranges[r], ranges[r + 1], flags] : [ranges[r], ranges[r + 1]])
  }

  return { ranges: result }
}

exports.constants = {
  /**
   * Range flag: The first byte of the stripped range becomes ';' instead of
   * a space, so ASI can't fold the previous statement into whatever follows.
   */
  SEMI: binding.SEMI,

  /**
   * Range flag: The first byte of the stripped range becomes ')' instead of
   * a space. Used in tandem with a wider strip range to relocate the closing
   * ')' of an arrow function's parameter list to the line where '=>' lives,
   * when the (now stripped) return-type annotation spanned newlines.
   */
  PAREN: binding.PAREN,

  /**
   * Range flag: The range marks non-erasable TypeScript syntax (enums,
   * namespaces, parameter properties, angle casts) that cannot be stripped
   * to valid JavaScript. strip() throws on these; lex() returns them so
   * callers can implement their own handling.
   */
  ERROR: binding.ERROR
}

function check(buffer, ranges) {
  for (let r = 0; r < ranges.length; r += 3) {
    if ((ranges[r + 2] & binding.ERROR) === 0) continue

    const start = ranges[r]
    const end = ranges[r + 1]

    let line = 1
    let column = 1

    for (let i = 0; i < start; i++) {
      if (buffer[i] === 0x0a) {
        line++
        column = 1
      } else {
        column++
      }
    }

    const source = Buffer.from(buffer.subarray(start, Math.min(end, start + 32))).toString()

    throw new SyntaxError(
      `Cannot strip non-erasable TypeScript syntax '${source}' at ${line}:${column}`
    )
  }
}

function apply(buffer, ranges) {
  const out = Buffer.from(buffer)

  // The lexer emits ranges in scan order: A single-byte flag patch that
  // overlaps a wider strip range (PAREN relocates ')' into a stripped span)
  // always follows that range, so applying in array order keeps the patched
  // byte intact.
  for (let r = 0; r < ranges.length; r += 3) {
    const s = ranges[r]
    const e = ranges[r + 1]
    const f = ranges[r + 2]

    if (f & binding.ERROR) continue

    for (let i = s; i < e; i++) {
      if (i === s) {
        if (f & binding.SEMI) {
          out[i] = 0x3b // ';'
          continue
        }
        if (f & binding.PAREN) {
          out[i] = 0x29 // ')'
          continue
        }
      }
      const c = out[i]
      if (c === 0x0a || c === 0x0d) continue
      out[i] = 0x20
    }
  }
  return out
}
