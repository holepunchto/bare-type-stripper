import Buffer, { BufferEncoding } from 'bare-buffer'

declare function strip(input: string | Buffer, encoding?: BufferEncoding, opts?: object): Buffer

declare namespace strip {
  export function lex(
    input: string | Buffer,
    encoding?: BufferEncoding,
    opts?: object
  ): {
    ranges: [start: number, end: number, flags?: number][]
  }

  export const constants: {
    SEMI: number
    PAREN: number
    ERROR: number
  }
}

export = strip
