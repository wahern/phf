# DEPENDENCIES #

* No runtime dependencies.
* The source is C++11 with C interfaces available.
* The included build requires GNU Make.

# BUILDING #

## Make Macros ##

The typical GNU macros can be used control the build.

### Compilation ###

* CXX: C++ compiler path.
* CXXFLAGS: C++ compiler flags.
* CPPFLAGS: C preprocessor flags.
* LDFLAGS: Linker flags. Not normally needed.
* SOFLAGS: Flags needed to build dynamic library.
* LOFLAGS: Flags needed to build loadable module. Normally should be the
  same as SOFLAGS, except on OS X.

### Installation ####
* prefix
* includedir
* libdir
* luacpath
* lua51cpath
* lua52cpath
* lua53cpath

## Make Targets ##

* phf: Builds command-line utility (development)
* libphf.so: Builds dynamic library for non-OS X
* libphf.dylib: Builds dynamic library for OS X
* lua5.1: Builds Lua 5.1 module at 5.1/phf.so. Lua 5.1 headers should be
  specified using CPPFLAGS if not in normal locations.
* lua5.2: Same as above, for Lua 5.2.
* lua5.3: Same as above, for Lua 5.3.

# USAGE #

## Lua ##

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
