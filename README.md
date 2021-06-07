µbgpsuite -- Micro BGP Suite and Utility library
================================================

# Introduction

`µbgpsuite`, the Micro BGP Suite and Utility library, is a project
aiming to provide a low level library and utilities for
high-performance and flexible network analysis, with special
focus on the Border Gateway Protocol (BGP).
The purpose of this suite is establishing a friendly environment
for research and experimentation of useful data study
techniques to improve network health.

# lonetix -- Low-overhead Networking programming Interface

The project is centered around `lonetix`, a low overhead and
low level networking library written in C.
It provides a set of general functionality to implement the suite utilities.
`lonetix` principles are:
- efficiency: `lonetix` has to be fast and versatile;
- predictability: data structures and functions should be predictable
  and reflect the actual protocol, abstraction should not degenerate
  into alienation;
- zero copy and zero overhead: be friendly to your target CPU and cache,
  you never know just how fast or poweful the target platform will be,
  ideally `lonetix` should be capable of performing useful work on embedded
  systems as well as full fledged power systems alike;
- lean: try to be self-contained and only introduce dependencies when strictly
  necessary.

Extensive documentation of `lonetix` and its API is available.

# Utilities

`lonetix` is the building block of `bgpgrep`, this far our single
utility -- but more of them are coming, right?

`bgpgrep` performs fast and reliable analysis of MRT dumps
collected by most Route Collecting projects. It takes a different
turn compared to most similar tools, in that it provides extensive
filtering utilities, in order to extrapolate only relevant data
out of each MRT dump (and incidentally save quite some time).
In-depth documentation of `bgpgrep` is available in its man page.

# License

The Micro BGP Suite is free software.
You can redistribute the `lonetix` library and/or modify it under the terms of the
GNU Lesser General Public License.
You can redistribute any utility and/or modify it under the terms of the
GNU General Public License.

The Micro BGP Suite is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the license terms for
more details.

See `COPYING.LESSER` for the GNU Lesser General Public License terms,
and `COPYING.GPL` for the GNU General Public License terms.

