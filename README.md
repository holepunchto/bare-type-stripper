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

See the [`bare-type-stripper` reference][reference].

[reference]: https://docs.pears.com/reference/bare/modules/bare-type-stripper

## What gets stripped

| Construct                 | Example                               |
| :------------------------ | :------------------------------------ |
| Type annotations          | `const x: number = 1`                 |
| Type aliases              | `type Foo = number`                   |
| Interfaces                | `interface Foo { x: number }`         |
| Type-only imports/exports | `import type { Foo } from 'mod'`      |
| Generics at declarations  | `function f<T>(x: T): T`              |
| Generics at call sites    | `foo<number>()`                       |
| Generic arrow functions   | `<T>(x: T) => x`                      |
| Type assertions           | `x as Foo`, `x satisfies Foo`         |
| Non-null assertion        | `obj!.foo`                            |
| Optional parameter marker | `function f(x?: T)`                   |
| Definite assignment       | `let x!: number`                      |
| Class member modifiers    | `public`, `private`, `readonly`, etc. |
| `implements` clauses      | `class C implements I`                |
| `declare` statements      | `declare const x: number`             |
| Overload signatures       | `function f(x: string): void`         |
| Abstract members          | `abstract foo(): void`                |

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
