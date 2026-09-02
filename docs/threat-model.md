# Threat model

## What this is

`bare-type-stripper` is compiled into Bare. It is listed in `src/builtins.json`, so every Bare process has it. That holds whether or not the process sealed, and no code has to load anything to reach it.

So this addon is part of Bare, and [Bare's threat model](https://github.com/holepunchto/bare/blob/main/docs/threat-model.md) covers it. Read that one first. This one only says where this addon sits in it.

## What it inherits

- **The promise.** Bare promises a sealed process gets no new native code. This addon is native code that is already in, so the seal neither adds it nor takes it away.
- **The attacker.** Untrusted JavaScript in a sealed process. It writes what it likes, runs on as many threads as it wants, and calls anything it can reach in any order and all at once. It can reach all of this addon.
- **The trust.** This addon is trusted, because Bare compiles it in. Whatever you compile in is your security policy, and this is one of the things you picked.
- **The walls.** The same table applies. A thread is not a wall and neither is a realm, so nothing here gets to assume it is alone.
- **The rules.** What Bare says to report, and what Bare says is not a bug, is the same here.

## What counts

- **Counts:** `binding.c` and the JavaScript that ships with it. Sealed JavaScript reaches all of it without loading a thing.
- **Does not count:** tests, benchmarks, and scratch code.

## What this addon adds

Nothing. One pure function. Source goes in, and the same source comes out with the type syntax replaced by spaces.

It reaches nothing and keeps nothing.

## Where the risk is

It is C walking source that an attacker chose, like the lexer, and the module system runs it on code it may not trust.

Its output is run, though, and that is the difference. Stripping rewrites the source, so a region stripped wrongly makes the program mean something else, and the source may have been trusted going in. Replacing with spaces keeps the positions and line numbers right, which keeps stack traces honest, but it does not make a wrong strip safe.

Stripping is heuristic on purpose. Leaving type syntax alone, or turning the input down, is a correctness matter. Stripping something that was not a type is not.

## What to report

- Any input where the output is valid JavaScript that means something different from the TypeScript it came from
- Reads outside the input while lexing, on any source at all, including truncated and invalid source
- Allocation or stack growth that an input can drive without bound
- Anything on Bare's report list

Not a bug: type syntax left in place, or input the stripper turns down.
