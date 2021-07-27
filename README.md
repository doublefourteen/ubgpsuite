µbgpsuite - Micro BGP Suite and Utility library
================================================

# Introduction

`µbgpsuite`, the Micro BGP Suite and Utility library, is a project
aiming to provide a low level library and utilities for
high-performance and flexible network analysis, with special
focus on the Border Gateway Protocol (BGP).
The purpose of this suite is establishing a friendly environment
for research and experimentation of useful data study
techniques to improve network health.

# lonetix - Low-overhead Networking programming Interface

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

`lonetix` is the building block of `bgpgrep` and `peerindex`, this far our
only two utilities - but more of them are coming, right?

`bgpgrep` performs fast and reliable analysis of MRT dumps
collected by most Route Collecting projects. It takes a different
turn compared to most similar tools, in that it provides extensive
filtering utilities, in order to extrapolate only relevant data
out of each MRT dump (and incidentally save quite some time).
In-depth documentation of `bgpgrep` is available in its man page.

`peerindex` allows to quickly inspect the peer index table record inside
MRT TABLE_DUMP_V2 RIBs. Just like `bgpgrep`, documentation is
available in a dedicated man page.

# Building

The Micro BGP Suite uses [Meson](https://mesonbuild.com/) to manage its build
process.

The basic steps to configure and build `lonetix` and every utility, with
debug options enabled, is:
```sh
$ git clone https://git.doublefourteen.io/bgp/ubgpsuite.git
$ cd ubgpsuite
$ meson build && cd build
$ ninja
```

In case you want to perform a release build, enable the release configuration,
like this:
```sh
$ git clone https://git.doublefourteen.io/bgp/ubgpsuite.git
$ cd ubgpsuite
$ meson --buildtype=release build && cd build
$ ninja
```

Or, alternatively, run the following command inside an already existing `build`
directory:
```sh
$ meson configure -Dbuildtype=release
$ ninja
```

Many build options are available to configure `µbgpsuite` build, you can use:
```sh
$ meson configure
```

to list them.
Refer to the [official documentation](https://mesonbuild.com/Manual.html)
for more advanced build management tasks.

# Documentation

The Micro BGP Suite uses [Doxygen](https://www.doxygen.org/index.html) to document its API.

In order to build `µbgpsuite` documentation you must enable the `build-doc`
configure flag. This flag is enabled automatically if you have `doxygen`
installed in your system when you run `meson` to configure the project for the
first time.

Once the flag is enabled, you can use:
```sh
$ ninja doc
```
inside build directory to generate Doxygen documentation.

You can access the documentation by pointing your web browser to
`doc/html/index.html` inside the build directory.

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

