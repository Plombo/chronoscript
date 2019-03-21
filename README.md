# ChronoScript

ChronoScript is a portable, lightweight scripting language designed for embedding in applications. In particular, it is intended for use in Chrono Crash, the planned successor to the [OpenBOR](https://github.com/DCurrent/openbor) game engine.

### What ChronoScript aims to be
* **Lightweight**: ChronoScript is meant to be statically linked into other applications, so its compiled size should be small. At the time of this writing, the `runscript` executable for x86_64 Linux is about 180 KB. It is also important that compiled scripts do not take up too much memory; this is a major shortcoming of OpenBOR script, ChronoScript's ancestor. ChronoScript limits the size of bytecode instructions to 8 bytes each, and does some simple compile-time optimizations to minimize the number of instructions per function.
* **Simple**: ChronoScript is meant to be accessible to novice programmers. It does not implement conceptually dense paradigms like object-oriented or functional programming. It is light on features, and can be mastered by someone who has never taken a computer science class.
* **Flexible**: This goal is at odds with the prior two, so adding flexibility to ChronoScript must be done in moderation. However, it is important that ChronoScript not be *so* lightweight and simple that it is cumbersome to do anything moderately complex. This is a problem that OpenBOR script has; ChronoScript addresses it by adding "object" (associative array) and "list" (dynamic array) types to OpenBOR script's available types.
* **Fast**: Since it is designed to be used in a game engine, where various scripts may be run on every update, scripts should take as little time as possible to execute. The performance of ChronoScript is competitive with its fellow lightweight scripting language Lua, which is among the fastest interpreted and dynamically typed languages.
* **Portable**: ChronoScript should work on any platform targeted by gcc or clang. Although it is written in C\++11, it does not use exceptions or even link with the standard C++ library; it depends only on the standard C library.

### Current status
ChronoScript is not yet feature complete, and is probably still full of bugs I don't know about yet. You are welcome to download it and try it out; just know that it's not yet suitable for any large or serious projects. If you find a bug, please do [report it](https://github.com/Plombo/chronoscript/issues).

### Authors
ChronoScript is developed by [Bryan Cain](https://github.com/Plombo). Parts of it are based on the OpenBOR script engine, which was originally written by uTunnels.

A few individual source files use code from other open source software projects:
* ralloc.c/h and RegAllocUtil.cpp/hpp are based on code from [Mesa](https://cgit.freedesktop.org/mesa/mesa).
* HashTable.cpp is a simplified version of cfuhash from [libcfu](http://libcfu.sourceforge.net/) by Don Owens.
* The hash table implementation in ScriptObject.cpp is based on the implementation of tables in [Lua](https://www.lua.org/).

### License
For now, ChronoScript is available under the terms of the GNU Lesser General Public License, version 3 or later. It will probably move to a more permissive license when development begins in earnest on the Chrono Crash game engine.
