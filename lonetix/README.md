Low-overhead Networking library Interface
=============================================

# Introdution and philosophy

`lonetix` is a general, performance oriented, C networking library.
Its field of application leans towards high-performance analysis of
Border Gateway Protocol (BGP) data.

`lonetix` is somewhat opinionated, its principles are:
- efficiency: `lonetix` has to be fast and versatile;
- predictability: data structures and functions should be predictable
  and reflect the actual protocol, abstraction should not degenerate
  into alienation;
- zero copy and zero overhead: be friendly to your target CPU and cache,
  you never know just how fast or poweful the target platform will be,
  ideally `lonetix` should be capable of performing useful work on embedded
  systems as well as full fledged power systems alike;
- lean: try to be self-contained and only introduce dependencies when
  necessary.

Following sections further elaborate on these points.

## Efficiency

Network analysis is usually thought as a computationally intensive task,
involving powerful machines capable of crunching large datasets.
Fast prototyping is tipically preferred over carefully planned, optimized code
and tools. Our belief is that careful optimizations, good algorithms and general
libraries may empower scientific research to productively elaborate data faster.

Efficient algorithms and tools make previously prohibitive tasks plausible.

Some researchers have no access to powerful workstations, though devices
commonly available to the general public are capable enough to perform
interesting network analysis tasks.

Good tools should not restrict research, they should encourage it.

Efficiency should be a guiding principle behind `lonetix`, and the main
reason for choosing C as a language.

## Predictability

`lonetix` is a relatively low-level C library. As such it deals with
common software engineering problems. In contrast with common opinion, C has
sufficient means to define a decent level of abstraction.
Powerful abstractions have to be formalized, documented, explained and learned.
Once this process is complete, powerful abstractions need to be used correctly.
Therefore, powerful abstractions need expensive engineering and comprehensive
documentation, they imply a learning curve and a period of practice.

Abstraction is a key software engineering concept only valuable if worthwhile.

Excessive abstraction may distract too much from the intent of a programming
interface, making it more obfuscated, less obvious, thus less predictable.
Additionally, it makes it harder for a programmer using them to guess or
estimate their performance penalty -- a particularly undesirable feature
in a scenario where such estimate could be crucial.
Whenever possible an interface should be transparent to the programmer,
essential, immediate in conveying its purpose and model, keep its field of
application clear and confined.

`lonetix` builds complex abstraction only in face of a sufficient gain.
Simplicity is a virtue, and obvious solutions need less explaination.
Oftentimes, solving a large problem with clear straight to the point code
is testament of a solid approach.

## Zero copy

`lonetix` deals, for the most part, with BGP messages.
Decoding them, ensuring their integrity and accessing their fields
conveniently and efficiently is central to the usefulness of the library.

Most libraries that read messages encoded in a particular protocol,
take the common approach of introducing a decode phase upfront,
with the intent of transforming its raw data into a more palatable
representation for the library.
The obvious advantages of this approach are:
- an efficient resulting data structure that makes it easy to access every
  message field when needed;
- any data integrity error is detected upfront, during the transformation.

Situation is specular for message writing.

This approach comes with its own set of disadvantages, though:
- an initial decoding phase implies the whole message is scanned at least
  once to organize it in the new data structure, even when only a single field of
  the entire message would be relevant to the user;
- CPU architectures greatly benefit from cache reuse, introducing a decode
  phase upfront that moves data around from a plain byte buffer to a complex
  data structure is usually bad news to the CPU cache;
- more data structures generally imply more memory allocations;
- translating raw data to the target data structure and back may
  require more complex API and implementation than providing equivalent
  facilities to access raw data directly.

These reasons motivated `lonetix` to explore a more trivial zero-copy approach:
whenever possible `lonetix` should work with raw BGP messages and require no
unnecessary data copy.

Do note that this approach is not perfect either. We simply believe that the
tradeoff is to `lonetix` advantage, and a zero copy approach fits better in
a performance-oriented and predictable library.

Same considerations apply to any portions of the library facing similar
situations (MRT data, other network protocols, etc...).

## Zero overhead

`lonetix` provides comprehensive facilities for network analysis and additional
utility functions for a wide variety of common tasks
(including string utilities, text parsing, etc...).
Library users should not be burdened with overhead for functionality they don't
need.

By design `lonetix` should be modular and require no runtime overhead
(such as background threads, `atexit()` hooks, or static initialization)
unless deemed as positively and unmistakably unavoidable.

`lonetix` is a **static library**, making it possible for the compiler to
strip any unused code from the resulting binary.

Careful coding should always allow to compile the library with
full optimizations on, including Link Time Optimization
(whether the binary should be optimized for space or
speed should be configurable by the user).

## Lean

No external dependency should be introduced unless strictly necessary.
This helps improving portability and makes `lonetix` usable even under
constrained environments.

`lonetix` **does not pursue strict ABI or API stability**.

Given that `lonetix` is a static library, keeping ABI stability is unnecessary.
Strict API stability tends to clutter libraries with large amounts of
legacy code, `lonetix` strives for incremental code improvement.
This sometimes calls for changes to the API and minor interface variations.
Users wishing for specific features from older versions that have been
evicted or changed on current ones, may fetch the older versions and link to
them.
Though API stability is not guaranteed, it should not be broken deliberately,
viable code migration paths should be offered when possible, for sensible use
cases.

# Documentation and examples

Complete project documentation is currently work in progress.

Extensive `Doxygen` API documentation is available for most of the library.
We also believe code should be clear, understandable and idiomatic, so
you can check out the code of any utility using `lonetix`
(for example `bgpgrep`) as a reference of how to take advantage of the library.

# License

`lonetix` is free software. You can redistribute it and/or modify it under
the terms of the GNU Lesser General Public License version 3 as published
by the Free Software Foundation, or, at your choice, any subsequent version
of the same license. `lonetix` is distributed in the hope that it will be
useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the license terms for
more details.

See `COPYING.LESSER` under the Micro BGP Suite root project directory for the
GNU Lesser General Public License terms, or see the
[Free Software Foundation website](https://www.gnu.org/licenses/lgpl-3.0.txt).

