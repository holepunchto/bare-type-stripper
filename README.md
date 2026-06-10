# bare-type-stripper

Heuristic lexer for stripping TypeScript type syntax to produce plain JavaScript. Stripped regions are replaced with spaces so source positions and line numbers are preserved.

## Usage

```js
const strip = require('bare-type-stripper')

strip(`
  const x: number = 1
  function f<T>(xs: T[]): T { return xs[0] }
`).toString()

// '
//   const x         = 1
//   function f   (xs   )    { return xs[0] }
// '
```

## API

#### `const output = strip(source[, encoding][, options])`

Strip TypeScript-only syntax from `source` and return plain JavaScript as a `Buffer`. Stripped regions are replaced with spaces (newlines preserved) so the output has the same byte length as the input, keeping stack traces and source positions aligned.

`source` may be a string or a `Buffer`. When a string, `encoding` selects the character encoding (defaults to `'utf8'`).

Throws a `SyntaxError` when the source contains non-erasable TypeScript syntax (see below).

Options are reserved.

#### `const ranges = strip.lex(source[, encoding][, options])`

Return the raw `[start, end, flags?]` ranges that `strip()` would erase, without applying them. The optional third element is a bitmap of flags from `strip.constants` that controls how a single byte in the range is rewritten (instead of being blanked to a space).

```js
strip.lex('const x: number = 1')
// [ [ 7, 16 ] ]
```

#### `strip.constants`

| Constant | Description                                                                                                                                                                                                                                                                                           |
| :------- | :---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `SEMI`   | The first byte of the range becomes `;` instead of a space. Used on whole-statement strips (`type`, `interface`, `declare`, `import type`, `export type/interface`) and on the first stripped modifier of a class member to keep ASI from folding the preceding statement or field into what follows. |
| `PAREN`  | The first byte of the range becomes `)` instead of a space. Used together with a wider strip range to relocate an arrow function's closing `)` down to the line where `=>` lives, when the (now stripped) return-type annotation spanned newlines.                                                    |
| `ERROR`  | The range marks non-erasable TypeScript syntax that cannot be stripped to valid JavaScript. `strip()` throws a `SyntaxError` when a lexed range carries this flag; `lex()` returns the range so callers can implement their own handling.                                                             |

## What gets stripped

| Construct                 | Example                               |
| :------------------------ | :------------------------------------ |
| Type annotations          | `const x: number = 1`                 |
| Type aliases              | `type Foo = number`                   |
| Interfaces                | `interface Foo { x: number }`         |
| Type-only imports/exports | `import type { Foo } from 'mod'`      |
| Generics at declarations  | `function f<T>(x: T): T`              |
| Generics at call sites    | `foo<number>()`                       |
| Generic arrow functions   | `<T,>(x: T) => x`, `<T>(x: T) => x`   |
| Type assertions           | `x as Foo`, `x satisfies Foo`         |
| Non-null assertion        | `obj!.foo`                            |
| Optional parameter marker | `function f(x?: T)`                   |
| Definite assignment       | `let x!: number`                      |
| Class member modifiers    | `public`, `private`, `readonly`, etc. |
| `implements` clauses      | `class C implements I`                |
| `declare` statements      | `declare const x: number`             |

## What is left alone

- Decorators - they emit runtime code and are valid JavaScript syntax.

## What throws

Constructs with runtime semantics that a purely lexical stripper cannot reproduce are marked with the `ERROR` flag, and `strip()` throws a `SyntaxError` when it meets one:

- `enum` / `const enum` declarations - they emit a runtime object.
- `namespace` / `module` declarations with bodies - they emit runtime code.
- Parameter properties - `constructor(public x: number)` implies a `this.x = x` assignment that stripping the modifier would silently lose.
- Old-style angle-bracket type assertions (`<Foo>expr`) - indistinguishable from JSX, which is not supported.

## Limitations

The stripper targets plain `.ts` sources; JSX (`.tsx`) is not supported and is reported as non-erasable syntax.

## License

Apache-2.0
