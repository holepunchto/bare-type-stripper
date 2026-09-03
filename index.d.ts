import Buffer, { BufferEncoding } from 'bare-buffer'

/**
 * Strip TypeScript-only syntax from `input` and return plain JavaScript as a `Buffer`. Stripped
 * regions are replaced with spaces (newlines preserved) so the output has the same byte length as
 * the input, keeping stack traces and source positions aligned.
 * @param input - The TypeScript source to strip, as a string or a `Buffer`. The input is copied
 * before it is lexed, so a buffer backed by a `SharedArrayBuffer` cannot change under the strip.
 * @param encoding - Encoding used to decode `input` when it is a string (default `'utf8'`); ignored
 * when `input` is already a `Buffer`.
 * @param opts - An options object; currently unused.
 * @throws {TypeError} `input` is neither a string nor a buffer.
 * @throws {SyntaxError} the source contains non-erasable TypeScript syntax (`enum`/`const enum`,
 * `namespace`/`module` with a body, parameter properties, or angle-bracket type assertions).
 */
declare function strip(input: string | Buffer, encoding?: BufferEncoding, opts?: object): Buffer

declare namespace strip {
  /**
   * Lex `input` and return the ranges `strip()` would replace, as `[start, end, flags?]` triples.
   * @param input - The TypeScript source to lex, as a string or a `Buffer`. A buffer backed by a
   * `SharedArrayBuffer` is copied before it is lexed; transfer a plain `ArrayBuffer` when that copy
   * matters.
   * @param encoding - Encoding used to decode `input` when it is a string (default `'utf8'`).
   * @param opts - An options object; currently unused.
   */
  export function lex(
    input: string | Buffer,
    encoding?: BufferEncoding,
    opts?: object
  ): [start: number, end: number, flags?: number][]

  export const constants: {
    SEMI: number
    PAREN: number
    ERROR: number
  }
}

export = strip
