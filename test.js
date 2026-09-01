const test = require('brittle')
const strip = require('.')

test('import type - default specifier', (t) => {
  eq(t, "import type Foo from 'mod'\nconst x = 1", ';                         \nconst x = 1')
})

test('import type - named specifiers', (t) => {
  eq(
    t,
    "import type { Foo, Bar } from 'mod'\nconst x = 1",
    ';                                  \nconst x = 1'
  )
})

test('import { type X, Y } - mixed leading', (t) => {
  eq(t, "import { type Foo, bar } from 'mod'", "import {           bar } from 'mod'")
})

test('import { Y, type X } - mixed trailing', (t) => {
  eq(t, "import { bar, type Foo } from 'mod'", "import { bar           } from 'mod'")
})

test('import { type X, type Y } - all type-only', (t) => {
  eq(t, "import { type Foo, type Bar } from 'mod'", "import {                    } from 'mod'")
})

test('import { type X } - only type-only', (t) => {
  eq(t, "import { type Foo } from 'mod'", "import {          } from 'mod'")
})

test('import { a, type X, b } - type-only in middle', (t) => {
  eq(t, "import { a, type Foo, b } from 'mod'", "import { a,           b } from 'mod'")
})

test('import { type X as Y, b } - aliased type-only', (t) => {
  eq(t, "import { type Foo as F, bar } from 'mod'", "import {                bar } from 'mod'")
})

test('import { "string" as X } - string-literal specifier name', (t) => {
  eq(t, "import { 'x' as C2 } from './m'", "import { 'x' as C2 } from './m'")
})

test('import { type X, "string" as Y } - type-only with string-literal sibling', (t) => {
  eq(t, "import { type Foo, 'x' as C2 } from './m'", "import {           'x' as C2 } from './m'")
})

test('export { X as "string" } - string-literal export name', (t) => {
  eq(t, "export { foo as 'x' }", "export { foo as 'x' }")
})

test('export type - type alias re-export', (t) => {
  eq(
    t,
    "export type { Foo } from 'mod'\nconst x = 1",
    ';                             \nconst x = 1'
  )
})

test('export type Foo = ...', (t) => {
  eq(t, 'export type Foo = number\nconst x = 1', ';                       \nconst x = 1')
})

test('export type alias with multi-line union', (t) => {
  eq(t, 'export type T =\n  | A\n  | B\nconst x = 1', ';              \n     \n     \nconst x = 1')
})

test('export type alias with object body', (t) => {
  eq(
    t,
    'export type T = {\n  readonly a?: (x: string) => Promise<void>;\n  readonly b?: number;\n}\nconst x = 1',
    ';                \n                                            \n                      \n \nconst x = 1'
  )
})

test('export type { T } without from clause', (t) => {
  eq(t, 'export type { T }\nconst x = 1', ';                \nconst x = 1')
})

test('export type { T } with newline before from', (t) => {
  eq(t, "export type { T }\nfrom 'mod'", ';                \n          ')
})

test('export type * re-export', (t) => {
  eq(t, "export type * from 'mod'\nconst x = 1", ';                       \nconst x = 1')
})

test('export type * as namespace re-export', (t) => {
  eq(t, "export type * as NS from 'mod'", ';                             ')
})

test('export { type Foo, bar }', (t) => {
  eq(t, 'export { type Foo, bar }', 'export {           bar }')
})

test('export { bar, type Foo }', (t) => {
  eq(t, 'export { bar, type Foo }', 'export { bar           }')
})

test('dynamic import', (t) => {
  eq(t, "const p = import('./foo.js')", "const p = import('./foo.js')")
})

test('import.meta', (t) => {
  eq(t, 'const u = import.meta.url', 'const u = import.meta.url')
})

test('type alias', (t) => {
  eq(t, 'type Foo = number\nconst x = 1', ';                \nconst x = 1')
})

test('type alias with generic', (t) => {
  eq(t, 'type Foo<T> = T | null\nconst x = 1', ';                     \nconst x = 1')
})

test('parenthesized type alias', (t) => {
  eq(t, 'type T = (void)\nconst x = 1', ';              \nconst x = 1')
})

test('tuple type with labels', (t) => {
  eq(
    t,
    'type T = [a: number, b: string]\nconst x = 1',
    ';                              \nconst x = 1'
  )
})

test('variance modifiers on type parameters', (t) => {
  eq(
    t,
    'type P<in T, out U> = (x: T) => U\nconst x = 1',
    ';                                \nconst x = 1'
  )
})

test('mapped type', (t) => {
  eq(
    t,
    'type M = { [K in keyof T]: T[K] }\nconst x = 1',
    ';                                \nconst x = 1'
  )
})

test('conditional type with infer', (t) => {
  eq(
    t,
    'type C = T extends infer U ? U : never\nconst x = 1',
    ';                                     \nconst x = 1'
  )
})

test('keyof and typeof type operators', (t) => {
  eq(t, 'type K = keyof typeof x\nconst x = 1', ';                      \nconst x = 1')
})

test('template literal type', (t) => {
  eq(t, 'type T = `a${string}b`\nconst x = 1', ';                     \nconst x = 1')
})

test('interface', (t) => {
  eq(
    t,
    'interface Foo { x: number; y: string }\nconst x = 1',
    ';                                     \nconst x = 1'
  )
})

test('interface extends', (t) => {
  eq(
    t,
    'interface Foo extends Bar<T> { x: number }\nconst x = 1',
    ';                                         \nconst x = 1'
  )
})

test('export interface', (t) => {
  eq(
    t,
    'export interface Foo { x: number }\nconst x = 1',
    ';                                 \nconst x = 1'
  )
})

test('const annotation', (t) => {
  eq(t, 'const x: number = 1', 'const x         = 1')
})

test('let annotation', (t) => {
  eq(t, 'let x: string = "a"', 'let x         = "a"')
})

test('var annotation with union', (t) => {
  eq(t, 'var x: number | string = 1', 'var x                  = 1')
})

test('qualified type name', (t) => {
  eq(t, 'const x: A.B.C = y', 'const x        = y')
})

test('constructor type in annotation', (t) => {
  eq(t, 'const f: new () => T = null as any', 'const f              = null       ')
})

test('import type query in annotation', (t) => {
  eq(t, 'let x: import("m").T = y', 'let x                = y')
})

test('annotation with multiline generic object type', (t) => {
  eq(
    t,
    'const x: ReadonlyArray<{\n  readonly f: (x: string) => boolean;\n}> = []',
    'const x                 \n                                     \n   = []'
  )
})

test('definite assignment !:', (t) => {
  eq(t, 'let x!: number\nx = 1', 'let x         \nx = 1')
})

test('multiple declarators', (t) => {
  eq(t, 'const x: number = 1, y: string = "a"', 'const x         = 1, y         = "a"')
})

test('destructuring with annotation', (t) => {
  eq(
    t,
    'const { a, b }: { a: number; b: string } = obj',
    'const { a, b }                           = obj'
  )
})

test('tuple destructure with annotation', (t) => {
  eq(t, 'const [a, b]: [number, string] = arr', 'const [a, b]                   = arr')
})

test('function with param annotation', (t) => {
  eq(t, 'function f(x: number) { return x }', 'function f(x        ) { return x }')
})

test('function with return annotation', (t) => {
  eq(
    t,
    'function f(x: number): string { return "" }',
    'function f(x        )         { return "" }'
  )
})

test('parenthesized return type', (t) => {
  eq(t, 'function f(): (void) {}', 'function f()         {}')
})

test('union return type without parens', (t) => {
  eq(t, 'var t = (): void | number => { }', 'var t = ()                => { }')
})

test('function generic', (t) => {
  eq(t, 'function f<T>(x: T): T { return x }', 'function f   (x   )    { return x }')
})

test('optional parameter', (t) => {
  eq(t, 'function f(x?: number) { return x }', 'function f(x         ) { return x }')
})

test('default parameter with annotation', (t) => {
  eq(t, 'function f(x: number = 1) { return x }', 'function f(x         = 1) { return x }')
})

test('default parameter before annotated parameter', (t) => {
  eq(t, 'function f(x = 1, y: number) { return y }', 'function f(x = 1, y        ) { return y }')
})

test('object default parameter before annotated parameter', (t) => {
  eq(t, 'function f(a = { b: 1 }, c: string) {}', 'function f(a = { b: 1 }, c        ) {}')
})

test('this parameter (only)', (t) => {
  eq(t, 'function f(this: Foo) { return this }', 'function f(         ) { return this }')
})

test('this parameter with siblings', (t) => {
  eq(
    t,
    'function f(this: Foo, x: number) { return x }',
    'function f(           x        ) { return x }'
  )
})

test('this parameter on class method', (t) => {
  eq(
    t,
    'class C { m(this: C, x: number) { return x } }',
    'class C { m(         x        ) { return x } }'
  )
})

test('this parameter on arrow', (t) => {
  eq(t, 'const f = (this: void, x: number) => x', 'const f = (            x        ) => x')
})

test('rest parameter with annotation', (t) => {
  eq(t, 'function f(...xs: number[]) { return xs }', 'function f(...xs          ) { return xs }')
})

test('parameter decorator with dotted name', (t) => {
  eq(t, 'function f(@A.B x) {}', 'function f(@A.B x) {}')
})

test('parameter decorator strips as cast in args', (t) => {
  eq(t, 'function f(@dec(x as any) y) {}', 'function f(@dec(x       ) y) {}')
})

test('parameter decorator strips generic args', (t) => {
  eq(t, 'function f(@dec<T> y) {}', 'function f(@dec    y) {}')
})

test('function overload signature', (t) => {
  eq(
    t,
    'function f(): number;\nfunction f(): any { return 1 }',
    ';                    \nfunction f()      { return 1 }'
  )
})

test('function overload signatures without semicolons', (t) => {
  eq(
    t,
    'function f(x: string): void\nfunction f(x: number): void\nfunction f(x) { return x }',
    ';                          \n;                          \nfunction f(x) { return x }'
  )
})

test('export function overload signatures', (t) => {
  eq(
    t,
    'export function f(x: string): void\nexport function f(x: number): void\nexport function f(x) { return x }',
    ';                                 \n;                                 \nexport function f(x) { return x }'
  )
})

test('export default function overload signature', (t) => {
  eq(
    t,
    'export default function f(x: string): void;\nexport default function f(x) { return x }',
    ';                                          \nexport default function f(x) { return x }'
  )
})

test('export function overload with async implementation', (t) => {
  eq(
    t,
    'export function f(x: string): Promise<string[]>;\nexport async function f(x: string): Promise<string[]> {\n  return []\n}',
    ';                                               \nexport async function f(x        )                    {\n  return []\n}'
  )
})

test('async function overload signature', (t) => {
  eq(
    t,
    'async function f(x: string): Promise<void>;\nasync function f(x: any): Promise<void> { return }',
    ';                                          \nasync function f(x     )                { return }'
  )
})

test('generator function overload signature', (t) => {
  eq(
    t,
    'function* f(x: string): Iterator<number>;\nfunction* f(x: any): Iterator<number> { yield 1 }',
    ';                                        \nfunction* f(x     )                   { yield 1 }'
  )
})

test('async generator function overload signature', (t) => {
  eq(
    t,
    'async function* f(x: string): AsyncIterator<number>;\nasync function* f(x: any): AsyncIterator<number> { yield 1 }',
    ';                                                   \nasync function* f(x     )                        { yield 1 }'
  )
})

test('export async function overload signature', (t) => {
  eq(
    t,
    'export async function f(x: string): Promise<void>;\nexport async function f(x: any): Promise<void> { return }',
    ';                                                 \nexport async function f(x     )                { return }'
  )
})

test('generic with extends clause', (t) => {
  eq(
    t,
    'function f<T extends string>(x: T): T { return x }',
    'function f                  (x   )    { return x }'
  )
})

test('default type parameter', (t) => {
  eq(
    t,
    'function f<T = string>(x: T): T { return x }',
    'function f            (x   )    { return x }'
  )
})

test('const type parameter', (t) => {
  eq(t, 'function f<const T>(x: T): T { return x }', 'function f         (x   )    { return x }')
})

test('asserts predicate return type', (t) => {
  eq(
    t,
    'function f(x: any): asserts x is number { }',
    'function f(x     )                      { }'
  )
})

test('is predicate return type', (t) => {
  eq(
    t,
    'function f(x: any): x is number { return true }',
    'function f(x     )              { return true }'
  )
})

test('is predicate with object type', (t) => {
  eq(
    t,
    'function f(value: unknown): value is { K: unknown } { return true }',
    'function f(value         )                          { return true }'
  )
})

test('asserts predicate with object type', (t) => {
  eq(
    t,
    'function f(x: unknown): asserts x is { K: unknown } { return }',
    'function f(x         )                              { return }'
  )
})

test('arrow is predicate with object type', (t) => {
  eq(
    t,
    'const f = (x: unknown): x is { a: A } => true',
    'const f = (x         )                => true'
  )
})

test('catch clause type annotation', (t) => {
  eq(t, 'try { f() } catch (e: unknown) { throw e }', 'try { f() } catch (e         ) { throw e }')
})

test('as Type', (t) => {
  eq(t, 'const x = y as number', 'const x = y          ')
})

test('as const', (t) => {
  eq(t, 'const x = [1, 2] as const', 'const x = [1, 2]         ')
})

test('satisfies Type', (t) => {
  eq(t, 'const x = { a: 1 } satisfies Foo', 'const x = { a: 1 }              ')
})

test('chained as', (t) => {
  eq(t, 'const x = y as any as Foo', 'const x = y              ')
})

test('chained satisfies', (t) => {
  eq(t, 'const x = a satisfies any satisfies number', 'const x = a                               ')
})

test('satisfies with generic type', (t) => {
  eq(t, 'const x = foo satisfies Bar<T>', 'const x = foo                 ')
})

test('as between bitwise operators', (t) => {
  eq(t, 'const x = a & b as any & T', 'const x = a & b           ')
})

test('as Type does not consume following comparison', (t) => {
  eq(t, 'const x = 1 + 1 as number > 2', 'const x = 1 + 1           > 2')
})

test('as with intersection object type', (t) => {
  eq(
    t,
    'if ((x as A & { b?: string }).b !== y.b) f()',
    'if ((x                      ).b !== y.b) f()'
  )
})

test('as with multi-line object type', (t) => {
  eq(
    t,
    'const value = {} as {\n  a: number\n  b: string\n}',
    'const value = {}     \n           \n           \n '
  )
})

test('chained as with multi-line object type', (t) => {
  eq(
    t,
    'const value = {} as unknown as {\n  a: number\n  b: string\n}',
    'const value = {}                \n           \n           \n '
  )
})

test('chained as ending in object type', (t) => {
  eq(
    t,
    'const v = x as unknown as { a: number }\nconst z = 1',
    'const v = x                            \nconst z = 1'
  )
})

test('postfix non-null', (t) => {
  eq(t, 'const x = obj!.foo', 'const x = obj .foo')
})

test('chained non-null', (t) => {
  eq(t, 'const x = a!.b!.c', 'const x = a .b .c')
})

test('logical not is preserved', (t) => {
  eq(t, 'const x = !y', 'const x = !y')
})

test('!= is preserved', (t) => {
  eq(t, 'const x = a != b', 'const x = a != b')
})

test('non-null before optional chain', (t) => {
  eq(t, 'const x = obj!?.foo', 'const x = obj ?.foo')
})

test('class with property annotation', (t) => {
  eq(t, 'class C { x: number = 1 }', 'class C { x         = 1 }')
})

test('class with private modifier', (t) => {
  eq(t, 'class C { private x = 1 }', 'class C { ;       x = 1 }')
})

test('class with public method', (t) => {
  eq(t, 'class C { public foo(): void {} }', 'class C { ;      foo()       {} }')
})

test('class with implements', (t) => {
  eq(t, 'class C implements I, J { foo() {} }', 'class C                 { foo() {} }')
})

test('class extends generic', (t) => {
  eq(t, 'class C extends Base<string> { foo() {} }', 'class C extends Base         { foo() {} }')
})

test('class generic parameters', (t) => {
  eq(t, 'class C<T> { foo(x: T): T { return x } }', 'class C    { foo(x   )    { return x } }')
})

test('readonly property', (t) => {
  eq(t, 'class C { readonly x: number = 1 }', 'class C { ;        x         = 1 }')
})

test('readonly private field', (t) => {
  eq(t, 'class C { readonly #x: T = 1 }', 'class C { ;        #x    = 1 }')
})

test('stacked modifiers before private field', (t) => {
  eq(t, 'class C { private readonly #x: T }', 'class C { ;                #x    }')
})

test('modifiers before string and numeric field names', (t) => {
  eq(t, "class C { readonly 'a': T; readonly 0: T }", "class C { ;        'a'   ; ;        0    }")
})

test('override modifier', (t) => {
  eq(t, 'class C { override foo() {} }', 'class C { ;        foo() {} }')
})

test('public static readonly stacked modifiers', (t) => {
  eq(
    t,
    'class C { public static readonly x: T = 1 }',
    'class C { ;      static ;        x    = 1 }'
  )
})

test('declare class member', (t) => {
  eq(t, 'class C { declare x: number }', 'class C { ;                 }')
})

test('declare class member with object type', (t) => {
  eq(t, 'class C { declare x: { a: number } }', 'class C { ;                        }')
})

test('optional method', (t) => {
  eq(t, 'class C { method?(v: any) {} }', 'class C { method (v     ) {} }')
})

test('index signature', (t) => {
  eq(t, 'class C { [key: string]: any }', 'class C {                    }')
})

test('getter with return annotation', (t) => {
  eq(t, 'class C { get g(): any { return 1 } }', 'class C { get g()      { return 1 } }')
})

test('setter with param annotation', (t) => {
  eq(t, 'class C { set g(v: any) {} }', 'class C { set g(v     ) {} }')
})

test('accessor modifier', (t) => {
  eq(t, 'class C { accessor x }', 'class C { accessor x }')
})

test('static accessor', (t) => {
  eq(t, 'class C { static accessor x }', 'class C { static accessor x }')
})

test('accessor field with annotation', (t) => {
  eq(t, 'class C { accessor x: number = 1 }', 'class C { accessor x         = 1 }')
})

test('constructor with parameter properties throws', (t) => {
  t.exception.all(
    () => strip('class C { constructor(public x: number, private y: string) {} }'),
    SyntaxError
  )
})

test('readonly parameter property throws', (t) => {
  t.exception.all(() => strip('class A { constructor(readonly x: number) {} }'), SyntaxError)
})

test('parameter named like a modifier', (t) => {
  eq(t, 'function f(readonly, override) {}', 'function f(readonly, override) {}')
})

test('function as property key', (t) => {
  eq(t, 'const o = { function: 1 }', 'const o = { function: 1 }')
})

test('class with method overloads + impl', (t) => {
  eq(
    t,
    'class C {\n  foo(x: number): number\n  foo(x: string): string\n  foo(x: any): any { return x }\n}',
    'class C {\n  ;                     \n  ;                     \n  foo(x     )      { return x }\n}'
  )
})

test('class method overloads with semicolons', (t) => {
  eq(
    t,
    'class C {\n  foo(x: number): number;\n  foo(x: string): string;\n  foo(x: any): any { return x }\n}',
    'class C {\n  ;                      \n  ;                      \n  foo(x     )      { return x }\n}'
  )
})

test('constructor overloads', (t) => {
  eq(
    t,
    'class C {\n  constructor(x: string);\n  constructor(x: number);\n  constructor(x) { this.x = x }\n}',
    'class C {\n  ;                      \n  ;                      \n  constructor(x) { this.x = x }\n}'
  )
})

test('class method overload with modifier', (t) => {
  eq(
    t,
    'class C {\n  public foo(x: number): number;\n  public foo(x: any): any { return x }\n}',
    'class C {\n  ;                             \n  ;      foo(x     )      { return x }\n}'
  )
})

test('static method overload', (t) => {
  eq(
    t,
    'class C {\n  static foo(x: number): number;\n  static foo(x: any): any { return x }\n}',
    'class C {\n  ;                             \n  static foo(x     )      { return x }\n}'
  )
})

test('async method overload', (t) => {
  eq(
    t,
    'class C {\n  m(x: string): Promise<void>;\n  async m(x: any): Promise<void> { return }\n}',
    'class C {\n  ;                           \n  async m(x     )                { return }\n}'
  )
})

test('async method overload signature', (t) => {
  eq(
    t,
    'class C {\n  async m(x: string): Promise<void>;\n  async m(x: any): Promise<void> { return }\n}',
    'class C {\n  ;                                 \n  async m(x     )                { return }\n}'
  )
})

test('generator method overload', (t) => {
  eq(
    t,
    'class C {\n  *m(x: string): Iterator<number>;\n  *m(x: any): Iterator<number> { yield 1 }\n}',
    'class C {\n  ;                               \n  *m(x     )                   { yield 1 }\n}'
  )
})

test('async generator method overload', (t) => {
  eq(
    t,
    'class C {\n  async *m(x: string): AsyncIterator<number>;\n  async *m(x: any): AsyncIterator<number> { yield 1 }\n}',
    'class C {\n  ;                                          \n  async *m(x     )                        { yield 1 }\n}'
  )
})

test('abstract class strips abstract keyword', (t) => {
  eq(t, 'abstract class A { abstract foo(): void }', '         class A { ;                    }')
})

test('abstract accessor signature', (t) => {
  eq(
    t,
    'abstract class A { abstract get x(): number }',
    '         class A { ;                        }'
  )
})

test('abstract property', (t) => {
  eq(t, 'abstract class A { abstract x: number }', '         class A { ;        x         }')
})

test('computed method strips annotations', (t) => {
  eq(
    t,
    'class C { ["k"](x: number): number { return x } }',
    'class C { ["k"](x        )         { return x } }'
  )
})

test('computed generator method strips return annotation', (t) => {
  eq(t, 'class C { *["k"](): any { yield 1 } }', 'class C { *["k"]()      { yield 1 } }')
})

test('computed accessor strips return annotation', (t) => {
  eq(
    t,
    'class C { get ["k"](): number { return 1 } }',
    'class C { get ["k"]()         { return 1 } }'
  )
})

test('class method named async', (t) => {
  eq(t, 'class C { async(x: number): void {} }', 'class C { async(x        )       {} }')
})

test('class private async method annotations', (t) => {
  eq(
    t,
    'class C { async #m(x: number): Promise<void> {} }',
    'class C { async #m(x        )                {} }'
  )
})

test('class private accessor annotations', (t) => {
  eq(t, 'class C { get #x(): number { return 1 } }', 'class C { get #x()         { return 1 } }')
})

test('static async generator method annotations', (t) => {
  eq(
    t,
    'class C { static async *g(x: number): AsyncIterator<void> {} }',
    'class C { static async *g(x        )                      {} }'
  )
})

test('modifier before generator method', (t) => {
  eq(
    t,
    'class C { public *g(x: number): Iterator<void> {} }',
    'class C { ;      *g(x        )                 {} }'
  )
})

test('static initialization block', (t) => {
  eq(t, 'class C { static { f("x") } }', 'class C { static { f("x") } }')
})

test('class member decorator strips as cast in args', (t) => {
  eq(t, 'class C { @dec(x as any) foo() {} }', 'class C { @dec(x       ) foo() {} }')
})

test('class member decorator strips generic args', (t) => {
  eq(t, 'class C { @dec<T>(x) foo() {} }', 'class C { @dec   (x) foo() {} }')
})

test('class member decorator strips parenthesized cast', (t) => {
  eq(t, 'class C { @(foo as any) bar() {} }', 'class C { @(foo       ) bar() {} }')
})

test('object method return annotation', (t) => {
  eq(
    t,
    'const o = { m(): Foo {\n    return {}\n  } }',
    'const o = { m()      {\n    return {}\n  } }'
  )
})

test('object method param annotation', (t) => {
  eq(
    t,
    'const o = { m(x: number): number { return x } }',
    'const o = { m(x        )         { return x } }'
  )
})

test('async object method annotations', (t) => {
  eq(
    t,
    'const o = { async m(x: T): Promise<U> { return g(x) } }',
    'const o = { async m(x   )             { return g(x) } }'
  )
})

test('generator object method return annotation', (t) => {
  eq(t, 'const o = { *gen(): any { yield 1 } }', 'const o = { *gen()      { yield 1 } }')
})

test('object accessor annotations', (t) => {
  eq(
    t,
    'const o = { get x(): number { return 1 }, set x(v: number) {} }',
    'const o = { get x()         { return 1 }, set x(v        ) {} }'
  )
})

test('object method generics', (t) => {
  eq(t, 'const o = { m<T>(x: T): T { return x } }', 'const o = { m   (x   )    { return x } }')
})

test('computed object method annotations', (t) => {
  eq(
    t,
    'const o = { ["k"](x: number): number { return x } }',
    'const o = { ["k"](x        )         { return x } }'
  )
})

test('object method between plain properties', (t) => {
  eq(
    t,
    'const o = { a: 1, m(x: string): void { g(x) }, b: 2 }',
    'const o = { a: 1, m(x        )       { g(x) }, b: 2 }'
  )
})

test('object method in nested object literal', (t) => {
  eq(
    t,
    'const o = { f() { return { g(): any { return 1 } } } }',
    'const o = { f() { return { g()      { return 1 } } } }'
  )
})

test('properties named like method prefixes', (t) => {
  eq(t, 'const o = { async: 1, get: 2, set: 3 }', 'const o = { async: 1, get: 2, set: 3 }')
})

test('object method named async', (t) => {
  eq(t, 'const o = { async(x: number): void {} }', 'const o = { async(x        )       {} }')
})

test('async generator object method annotations', (t) => {
  eq(
    t,
    'const o = { async *m(x: number): AsyncIterator<void> {} }',
    'const o = { async *m(x        )                      {} }'
  )
})

test('arrow function param annotation', (t) => {
  eq(t, 'const f = (x: number) => x + 1', 'const f = (x        ) => x + 1')
})

test('arrow function return annotation', (t) => {
  eq(t, 'const f = (x: number): number => x + 1', 'const f = (x        )         => x + 1')
})

test('arrow function in annotation (function type)', (t) => {
  eq(
    t,
    'const f: (x: number) => string = null as any',
    'const f                        = null       '
  )
})

test('arrow generic <T,>', (t) => {
  eq(t, 'const f = <T,>(x: T) => x', 'const f =     (x   ) => x')
})

test('arrow generic <T>', (t) => {
  eq(t, 'const f = <T>(x: T) => x', 'const f =    (x   ) => x')
})

test('arrow generic with extends', (t) => {
  eq(t, 'const f = <T extends unknown>(x: T) => x', 'const f =                    (x   ) => x')
})

test('async arrow with generic', (t) => {
  eq(t, 'const f = async <T>(v: T) => {}', 'const f = async    (v   ) => {}')
})

test('annotation after expression statement in arrow body', (t) => {
  eq(
    t,
    'const f = () => {\n  foo()\n  const x: number = 1\n}',
    'const f = () => {\n  foo()\n  const x         = 1\n}'
  )
})

test('type alias inside arrow body', (t) => {
  eq(
    t,
    'const f = () => {\n  type T = number\n  return 1\n}',
    'const f = () => {\n  ;              \n  return 1\n}'
  )
})

test('annotation after expression statement in if block', (t) => {
  eq(
    t,
    'if (a) {\n  foo()\n  const x: number = 1\n}',
    'if (a) {\n  foo()\n  const x         = 1\n}'
  )
})

test('annotations in try and finally blocks', (t) => {
  eq(
    t,
    'try {\n  foo()\n  const x: string = s\n} finally {\n  bar()\n  let y: number = 1\n}',
    'try {\n  foo()\n  const x         = s\n} finally {\n  bar()\n  let y         = 1\n}'
  )
})

test('interface inside else block', (t) => {
  eq(
    t,
    'if (a) {\n  foo()\n} else {\n  bar()\n  interface I { x: number }\n}',
    'if (a) {\n  foo()\n} else {\n  bar()\n  ;                        \n}'
  )
})

test('ternary with parenthesized consequent and arrow alternate', (t) => {
  eq(t, 'const f = c ? (a) : b => c', 'const f = c ? (a) : b => c')
})

test('ternary with typed arrow consequent', (t) => {
  eq(t, 'const f = c ? (a: T) => a : b', 'const f = c ? (a   ) => a : b')
})

test('generic call site', (t) => {
  eq(t, 'const x = foo<number>()', 'const x = foo        ()')
})

test('generic call after method chain', (t) => {
  eq(t, 'const x = foo().bar<T>()', 'const x = foo().bar   ()')
})

test('generic type arguments on new', (t) => {
  eq(t, 'const x = new Foo<T>()', 'const x = new Foo   ()')
})

test('generic call after subscript', (t) => {
  eq(t, 'const x = arr[0]<T>()', 'const x = arr[0]   ()')
})

test('optional chain generic call', (t) => {
  eq(t, 'const x = foo?.<T>()', 'const x = foo?.   ()')
})

test('generic instantiation on parenthesized non-null', (t) => {
  eq(t, 'let a = (foo!)<any>', 'let a = (foo )     ')
})

test('tagged template with generic', (t) => {
  eq(t, 'const x = foo<T>`tagged ${"x"}`', 'const x = foo   `tagged ${"x"}`')
})

test('comparison not mistaken for generic', (t) => {
  eq(t, 'const x = a < b && c > d', 'const x = a < b && c > d')
})

test('comparison at end of input not mistaken for generic', (t) => {
  eq(t, 'module.exports = a < b', 'module.exports = a < b')
})

test('comparison inside if condition not mistaken for generic', (t) => {
  eq(t, 'if (a < b) {\n  foo()\n}', 'if (a < b) {\n  foo()\n}')
})

test('call-site generic with arrow type argument', (t) => {
  eq(t, 'const x = foo<(a: T) => U>()', 'const x = foo             ()')
})

test('union and intersection types', (t) => {
  eq(t, 'const x: A & B | C = 1', 'const x            = 1')
})

test('nested object type', (t) => {
  eq(
    t,
    'const x: { a: { b: number } } = { a: { b: 1 } }',
    'const x                       = { a: { b: 1 } }'
  )
})

test('object literal with as inside value', (t) => {
  eq(t, 'const o = { x: 1, y: foo as Bar }', 'const o = { x: 1, y: foo        }')
})

test('typed const in for-of', (t) => {
  eq(
    t,
    'for (const x: number of arr) { console.log(x) }',
    'for (const x         of arr) { console.log(x) }'
  )
})

test('enum throws', (t) => {
  t.exception.all(() => strip('enum E { A, B }\nconst x = 1'), SyntaxError)
})

test('const enum throws', (t) => {
  t.exception.all(() => strip('const enum E { A, B }'), SyntaxError)
})

test('export enum throws', (t) => {
  t.exception.all(() => strip('export enum E { A, B }'), SyntaxError)
})

test('enum lexes as a single error range', (t) => {
  const ranges = strip.lex('enum E { A, B }')
  t.alike(ranges, [[0, 15, strip.constants.ERROR]])
})

test('namespace throws', (t) => {
  t.exception.all(() => strip('namespace N { export const x = 1 }'), SyntaxError)
})

test('module declaration throws', (t) => {
  t.exception.all(() => strip('module N.M { const x = 1 }'), SyntaxError)
})

test('module.exports', (t) => {
  eq(t, 'module.exports = 1', 'module.exports = 1')
})

test('namespace identifier', (t) => {
  eq(t, 'const namespace = 1\nnamespace.foo()', 'const namespace = 1\nnamespace.foo()')
})

test('old-style angle cast throws', (t) => {
  t.exception.all(() => strip('const x = <number>y'), SyntaxError)
})

test('error message includes position', (t) => {
  t.exception.all(() => strip('const a = 1\nconst x = <number>y'), /at 2:11/)
})

test('object literal "type" property', (t) => {
  eq(t, "const obj = { type: 'foo', interface: 1 }", "const obj = { type: 'foo', interface: 1 }")
})

test('property access .type', (t) => {
  eq(t, 'const x = node.type', 'const x = node.type')
})

test('template literal contents untouched', (t) => {
  eq(t, 'const x = `value is ${y} as Foo`', 'const x = `value is ${y} as Foo`')
})

test('template literal, as inside ${...}', (t) => {
  eq(t, 'const x = `${y as Foo}`', 'const x = `${y       }`')
})

test('template literal, satisfies inside ${...}', (t) => {
  eq(t, 'const x = `${a satisfies T}`', 'const x = `${a            }`')
})

test('template literal, non-null inside ${...}', (t) => {
  eq(t, 'const x = `${obj!.foo}`', 'const x = `${obj .foo}`')
})

test('template literal, nested template ts inside ${...}', (t) => {
  eq(
    t,
    'const x = `outer ${a + `inner ${b as Bar}`}`',
    'const x = `outer ${a + `inner ${b       }`}`'
  )
})

test('regex contents untouched', (t) => {
  eq(t, 'const r = /type Foo = number/', 'const r = /type Foo = number/')
})

test('line comment contents untouched', (t) => {
  eq(t, '// const x: number = 1\nconst y = 1', '// const x: number = 1\nconst y = 1')
})

test('block comment contents untouched', (t) => {
  eq(t, '/* type Foo = number */\nconst y = 1', '/* type Foo = number */\nconst y = 1')
})

test('strip accepts Buffer input', (t) => {
  eq(t, Buffer.from('const x: number = 1'), 'const x         = 1')
})

test('declare let', (t) => {
  eq(t, 'declare let a', ';            ')
})

test('declare class', (t) => {
  eq(t, 'declare class C {}', ';                 ')
})

test('declare enum', (t) => {
  eq(t, 'declare enum E {}', ';                ')
})

test('declare namespace', (t) => {
  eq(t, 'declare namespace N {}', ';                     ')
})

test('declare module', (t) => {
  eq(t, 'declare module "./a" {}', ';                      ')
})

test('declare global', (t) => {
  eq(t, 'declare global {}', ';                ')
})

test('declare function return type', (t) => {
  eq(t, 'declare function f(): void', ';                         ')
})

test('export declare', (t) => {
  eq(t, 'export declare const x: number', ';                             ')
})

test('export declare with following statement', (t) => {
  eq(t, 'export declare let a\nconst x = 1', ';                   \nconst x = 1')
})

test('asi, class field, then modifier + computed member', (t) => {
  eq(
    t,
    'class C {\n    field = 1\n    public ["key"] = 2\n}',
    'class C {\n    field = 1\n    ;      ["key"] = 2\n}'
  )
})

test('asi, chained modifiers only need one semi', (t) => {
  eq(
    t,
    'class C {\n    field = 1\n    public readonly x = 2\n}',
    'class C {\n    field = 1\n    ;               x = 2\n}'
  )
})

test('asi, static readonly skips semi after static', (t) => {
  eq(t, 'class C { f = 1\nstatic readonly x = 2 }', 'class C { f = 1\nstatic          x = 2 }')
})

test('asi, declare member', (t) => {
  eq(
    t,
    'class C {\n    field = 1\n    declare x: number\n}',
    'class C {\n    field = 1\n    ;                \n}'
  )
})

test('asi, top-level interface', (t) => {
  eq(t, 'foo()\ninterface I {}\n[bar]', 'foo()\n;             \n[bar]')
})

test('asi, top-level type alias', (t) => {
  eq(t, 'foo()\ntype T = number\n[bar]', 'foo()\n;              \n[bar]')
})

test('asi, top-level import type', (t) => {
  eq(t, "foo()\nimport type X from 'm'\n[bar]", 'foo()\n;                     \n[bar]')
})

test('asi, as Type before (', (t) => {
  eq(t, 'foo as string\n(1)', 'foo ;        \n(1)')
})

test('asi, satisfies Type before (', (t) => {
  eq(t, 'foo satisfies string\n(1)', 'foo ;               \n(1)')
})

test('asi, satisfies Type before [', (t) => {
  eq(t, 'foo satisfies string\n[0]', 'foo ;               \n[0]')
})

test('asi, as Type in initializer before (', (t) => {
  eq(t, 'let car = 1 as number\n(1)', 'let car = 1 ;        \n(1)')
})

test('asi, satisfies Type before + needs no semi', (t) => {
  eq(t, 'foo satisfies string\n+ ""', 'foo                 \n+ ""')
})

test('asi, arrow with multi-line return type relocates )', (t) => {
  eq(
    t,
    'let f = (a: string, b: string): Array<\n   string\n> => [a, b];',
    'let f = (a        , b                 \n         \n) => [a, b];'
  )
})

test('asi, arrow with newline before return type relocates )', (t) => {
  eq(t, 'const f = ()\n:any => 1', 'const f = ( \n   ) => 1')
})

test('asi, arrow with newlines around params and return type relocates )', (t) => {
  eq(t, 'let g = (\n)\n:any => 1', 'let g = (\n \n   ) => 1')
})

test('type alias with leading-newline union', (t) => {
  eq(t, 'type T =\n  | A\n  | B\nconst x = 1', ';       \n     \n     \nconst x = 1')
})

test('type alias with trailing-operator union', (t) => {
  eq(t, 'type T = A |\n  B |\n  C\nconst x = 1', ';           \n     \n   \nconst x = 1')
})

test('type alias with next-line-operator union', (t) => {
  eq(t, 'type T = A\n  | B\n  | C\nconst x = 1', ';         \n     \n     \nconst x = 1')
})

test('type alias with intersection across lines', (t) => {
  eq(t, 'type T = A\n  & B\nconst x = 1', ';         \n     \nconst x = 1')
})

test('type alias with multi-line conditional', (t) => {
  eq(
    t,
    'type T = U extends\n  V\n  ? A\n  : B\nconst x = 1',
    ';                 \n   \n     \n     \nconst x = 1'
  )
})

test('type alias with multi-line object type', (t) => {
  eq(t, 'type T = {\n  x: number\n}\nconst x = 1', ';         \n           \n \nconst x = 1')
})

test('var-decl chain without semicolons strips both annotations', (t) => {
  eq(t, 'const x: number = 1\nconst y: number = 2', 'const x         = 1\nconst y         = 2')
})

test('let chain without semicolons strips both annotations', (t) => {
  eq(t, 'let x: number = 1\nlet y: number = 2', 'let x         = 1\nlet y         = 2')
})

test('decorator before type alias on next line', (t) => {
  eq(t, '@dec\ntype T = number\nconst x = 1', '@dec\n;              \nconst x = 1')
})

test('decorator before interface on next line', (t) => {
  eq(
    t,
    '@dec\ninterface I { x: number }\nconst x = 1',
    '@dec\n;                        \nconst x = 1'
  )
})

test('decorator with generic args', (t) => {
  eq(t, '@Object.freeze<any>\nclass B {}', '@Object.freeze     \nclass B {}')
})

test('decorator with parenthesized generic args', (t) => {
  eq(t, '@(Object.freeze<any>)\nclass D {}', '@(Object.freeze     )\nclass D {}')
})

test('decorator with as cast', (t) => {
  eq(t, '@(null as any)\nclass E {}', '@(null       )\nclass E {}')
})

test('deeply nested input does not crash', (t) => {
  const src = '{'.repeat(1000) + '}'.repeat(1000)
  t.execution(() => strip(src))
})

test('deeply nested parens lex without quadratic blowup', (t) => {
  const src = 'x = ' + '('.repeat(10000) + '1' + ')'.repeat(10000)
  t.execution(() => strip(src))
})

test('lex returns ranges', (t) => {
  const ranges = strip.lex('const x: number = 1')
  t.alike(ranges, [[7, 16]])
})

test('lex on plain code returns no ranges', (t) => {
  const ranges = strip.lex('const x = 1')
  t.alike(ranges, [])
})

test('lex emits semi flag for statement-level strips', (t) => {
  const ranges = strip.lex('type T = number')
  t.alike(ranges, [[0, 15, strip.constants.SEMI]])
})

test('lex emits a single range for an overload signature', (t) => {
  const ranges = strip.lex('function f(x: string): void;')
  t.alike(ranges, [[0, 28, strip.constants.SEMI]])
})

test('lex emits paren flag for relocated )', (t) => {
  const src = 'let f = (a: string, b: string): Array<\n   string\n> => [a, b];'
  const ranges = strip.lex(src)
  const last = ranges[ranges.length - 1]
  t.is(last[1] - last[0], 1)
  t.is(last[2] & strip.constants.PAREN, strip.constants.PAREN)
})

test('specifier list with a non-specifier byte terminates', (t) => {
  eq(t, 'import{@', 'import{@')
  eq(t, 'import {\0', 'import {\0')
  eq(t, 'x = 1;import{@', 'x = 1;import{@')
})

test('export specifier list with a star terminates', (t) => {
  eq(t, 'export{*', 'export{*')
})

test('class body truncated after a private name', (t) => {
  eq(t, 'class{#', 'class{#')
  eq(t, 'class C {#', 'class C {#')
})

test('unclosed parameter lists do not rescan the tail', (t) => {
  const src = '('.repeat(256 * 1024)
  eq(t, src, src)
})

test('input must be a string or buffer', (t) => {
  for (const input of [undefined, null, 123, {}, [], () => {}, true]) {
    t.exception.all(() => strip(input), /must be a string or buffer/)
  }
})

test('every view strips its whole byte range', (t) => {
  const source = Buffer.alloc(24)
  source.write('const x: number = 1')

  const expected = 'const x         = 1\0\0\0\0\0'

  const { buffer, byteOffset } = source

  for (const view of [
    source,
    new Uint8Array(buffer, byteOffset, 24),
    new Int8Array(buffer, byteOffset, 24),
    new Uint8ClampedArray(buffer, byteOffset, 24),
    new Uint16Array(buffer, byteOffset, 12),
    new Int16Array(buffer, byteOffset, 12),
    new Uint32Array(buffer, byteOffset, 6),
    new Int32Array(buffer, byteOffset, 6),
    new Float32Array(buffer, byteOffset, 6),
    new Float64Array(buffer, byteOffset, 3),
    new BigInt64Array(buffer, byteOffset, 3),
    new DataView(buffer, byteOffset, 24)
  ]) {
    t.is(strip(view).toString(), expected)
  }
})

test('input must not be backed by a shared array buffer', (t) => {
  const buffer = new SharedArrayBuffer(24)
  new Uint8Array(buffer).set(Buffer.from('const x: number = 1'.padEnd(24)))

  for (const view of [new Uint8Array(buffer), new Uint16Array(buffer), new DataView(buffer)]) {
    t.exception.all(() => strip(view), /must be a buffer/)
  }
})

test('a view strips only its own range', (t) => {
  const buffer = Buffer.from('let a: A = 1;const x: number = 1;let b: B = 2')

  const offset = 'let a: A = 1;'.length
  const length = 'const x: number = 1'.length

  const expected = 'const x         = 1'

  t.is(
    strip(new Uint8Array(buffer.buffer, buffer.byteOffset + offset, length)).toString(),
    expected
  )
  t.is(strip(new DataView(buffer.buffer, buffer.byteOffset + offset, length)).toString(), expected)
})

test('empty input', (t) => {
  t.is(strip(Buffer.alloc(0)).toString(), '')
  t.is(strip(new Uint8Array(0)).toString(), '')
  t.is(strip(new DataView(new ArrayBuffer(0))).toString(), '')
  t.is(strip('').toString(), '')
})

test('binding requires a buffer', (t) => {
  const binding = require('#binding')

  for (const buffer of [
    undefined,
    null,
    123,
    {},
    'source',
    new SharedArrayBuffer(8),
    new Uint8Array(8),
    new DataView(new ArrayBuffer(8))
  ]) {
    t.exception.all(() => binding.lex(buffer, 0, 8), /must be a buffer/)
  }
})

test('binding requires a numeric offset and length', (t) => {
  const binding = require('#binding')

  const buffer = new ArrayBuffer(8)

  for (const offset of [undefined, null, {}, '0', () => {}]) {
    t.exception.all(() => binding.lex(buffer, offset, 8), /Offset must be a number/)
  }

  for (const length of [undefined, null, {}, '8', () => {}]) {
    t.exception.all(() => binding.lex(buffer, 0, length), /Length must be a number/)
  }
})

test('binding bounds checks the offset and length', (t) => {
  const binding = require('#binding')

  const buffer = new ArrayBuffer(8)

  for (const [offset, length] of [
    [-1, 0],
    [0, -1],
    [0, 9],
    [8, 1],
    [9, 0],
    [4, 5],
    [Number.MAX_SAFE_INTEGER, 0],
    [0, Number.MAX_SAFE_INTEGER]
  ]) {
    t.exception.all(() => binding.lex(buffer, offset, length), /out of bounds/)
  }

  t.alike(binding.lex(buffer, 0, 8), new Uint32Array(0))
  t.alike(binding.lex(buffer, 8, 0), new Uint32Array(0))
})

test('binding ranges are relative to the offset', (t) => {
  const binding = require('#binding')

  const buffer = Buffer.from('let a: A = 1;const x: number = 1')

  const offset = 'let a: A = 1;'.length
  const length = 'const x: number = 1'.length

  t.alike(
    binding.lex(buffer.buffer, buffer.byteOffset + offset, length),
    new Uint32Array([7, 16, 0])
  )
})

test('binding treats a detached array buffer as empty', (t) => {
  const binding = require('#binding')

  const buffer = new ArrayBuffer(8)

  structuredClone(buffer, { transfer: [buffer] })

  t.alike(binding.lex(buffer, 0, 0), new Uint32Array(0))
  t.exception.all(() => binding.lex(buffer, 0, 8), /out of bounds/)
})

test('binding does not run JavaScript while validating', (t) => {
  const binding = require('#binding')

  const trap = new Proxy(
    {},
    {
      get: () => t.fail('trap ran'),
      has: () => t.fail('trap ran')
    }
  )

  t.exception.all(() => binding.lex(trap, 0, 8), /must be a buffer/)
  t.exception.all(() => binding.lex(new ArrayBuffer(8), trap, 8), /Offset must be a number/)
  t.exception.all(() => binding.lex(new ArrayBuffer(8), 0, trap), /Length must be a number/)
})

function eq(t, input, expected) {
  t.is(strip(input).toString(), expected)
}
