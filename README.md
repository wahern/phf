# Introduction #

This is a simple implementation of the CHD perfect hash algorithm. For
more information about the algorithm, see http://cmph.sourceforge.net/chd.html.

# Dependencies #

* No runtime dependencies.
* Requires a modern C++ compiler to build.
* The included build requires GNU Make.

# Building #

## Make Macros ##

The typical GNU macros can be used control the build.

### Compilation ###

Note that the modules for Lua 5.1, 5.2, and 5.3 can be built simultaneously.

* CXX: C++ compiler path.
* CXXFLAGS: C++ compiler flags.
* CPPFLAGS: C preprocessor flags. Necessary if Lua API cannot be discovered
  automatically. You can specify multiple include paths if building more than
  one Lua module.
* LDFLAGS: Linker flags. Not normally needed.
* SOFLAGS: Flags needed to build dynamic library.
* LOFLAGS: Flags needed to build loadable module. Normally should be the
  same as SOFLAGS, except on OS X.

### Installation ####
* prefix
* includedir
* libdir
* luacpath: Lua C module install path. Can be used for one-shot installation
  of a particular Lua version module.
* lua51cpath: Lua 5.1 C module install path.
* lua52cpath: Same as above, for 5.2.
* lua53cpath: Same as above, for 5.3.

## Make Targets ##

* phf: Builds command-line utility (development)
* libphf.so: Builds dynamic library for non-OS X
* libphf.dylib: Builds dynamic library for OS X
* lua5.1: Builds Lua 5.1 module at 5.1/phf.so. Lua 5.1 headers should be
  specified using CPPFLAGS if not in normal locations.
* lua5.2: Same as above, for Lua 5.2.
* lua5.3: Same as above, for Lua 5.3.

# Usage #

## Lua ##

## API ###

### phf.new(keys[, lamba][, alpha][, seed][, nodiv]) ###

* keys:array: array of keys in order from 1..#keys. They should be all numbers or
  all strings. Because the underlying C++ implementation does not tolerate
  duplicate keys (could enter an infinite loop if your unlucky, or raise
  SIGABORT if your lucky), an intermediate hash is first built to filter out
  duplicates.

* lambda:integer: number of keys per bucket when generating the g() function mapping.

* alpha:integer: output hash space loading factor in percentages from 1..100. 100%
  generates a *minimal* perfect hash function. But note that that the
  implementation does implement the necessary optimizations to ensure timely
  generation of a minimal perfect hash function. Normally you want a loading
  factor of 80% or 90%.

* seed:integer: random integer seed.

* nodiv:boolean: round r and m to powers of 2 and do modular reduction using bitwise
  AND. Note that the rounding occurs after calculation of the intermediate
  and output hash table loading. (This is more important when using the C
  interface for small hash tables. The optimization is substantial when the
  compiler can inline the code, but isn't substantial from Lua.)

Returns a callable object.

### phf:hash(key)

* Returns an integer hash for key in range 1..phf:m().

### Example ###

```Lua
local phf = require"phf"

local lambda = 4 -- how many keys per intermediate bucket
local alpha = 80 -- output hash space loading in percentage.

local keys = { "apple", "banana", "cherry", "date", "eggplant", "fig",
               "guava", "honeydew", "jackfruit", "kiwi", "lemon", "mango" }

local F = phf.new(keys, lambda, alpha)

for i=1,#keys do
	print(keys[i], F(keys[i]))
end

```
