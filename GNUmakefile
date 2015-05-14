all: # default target

-include .config

prefix ?= /usr/local
includedir ?= $(prefix)/include
libdir ?= $(prefix)/lib
bindir ?= $(prefix)/bin

luacpath ?=
lua51cpath ?=
lua52cpath ?=
lua53cpath ?=

RMDIR ?= rmdir
MKDIR ?= mkdir
CP ?= cp

OSNAME ?= $(shell uname -s)
CXXNAME ?= $(shell env CC=$(CXX) mk/luapath ccname)

ifeq ($(CXXNAME), sunpro)
CXXFLAGS ?= -g
else
CXXFLAGS ?= -std=c++11 -fno-rtti -fno-exceptions -g -O3 -Wall -Wextra
endif

ifeq ($(CXXNAME), sunpro)
SOFLAGS ?= -xcode=pic13 -shared
else
ifeq ($(OSNAME), Darwin)
SOFLAGS ?= -fPIC -dynamiclib -undefined dynamic_lookup
else
SOFLAGS ?= -fPIC -shared
endif
endif

ifeq ($(CXXNAME), sunpro)
LOFLAGS ?= -xcode=pic13 -shared
else
ifeq ($(OSNAME), Darwin)
LOFLAGS ?= -fPIC -bundle -undefined dynamic_lookup
else
LOFLAGS ?= -fPIC -shared
endif
endif

ifeq ($(OSNAME), Darwin)
LIBPHF ?= libphf.dylib
else
LIBPHF ?= libphf.so
endif

.PHONY: config

config:
	printf 'prefix ?= $(value prefix)'"\n" >| .config
	printf 'includedir ?= $(value includedir)'"\n" >> .config
	printf 'libdir ?= $(value libdir)'"\n" >> .config
	printf 'bindir ?= $(value bindir)'"\n" >> .config
	printf 'luacpath ?= $(value luacpath)'"\n" >> .config
	printf 'lua51cpath ?= $(value lua51cpath)'"\n" >> .config
	printf 'lua52cpath ?= $(value lua52cpath)'"\n" >> .config
	printf 'lua53cpath ?= $(value lua53cpath)'"\n" >> .config
	printf 'CXX ?= $(value CXX)'"\n" >> .config
	printf 'CPPFLAGS ?= $(value CPPFLAGS)'"\n" >> .config
	printf 'CXXFLAGS ?= $(value CXXFLAGS)'"\n" >> .config
	printf 'LDFLAGS ?= $(value LDFLAGS)'"\n" >> .config
	printf 'SOFLAGS ?= $(value SOFLAGS)'"\n" >> .config
	printf 'LOFLAGS ?= $(value LOFLAGS)'"\n" >> .config
	printf 'LIBS ?= $(value LIBS)'"\n" >> .config
	printf 'LIBPHF ?= $(value LIBPHF)'"\n" >> .config
	printf 'RM ?= $(value RM)'"\n" >> .config
	printf 'RMDIR ?= $(value RMDIR)'"\n" >> .config
	printf 'MKDIR ?= $(value MKDIR)'"\n" >> .config
	printf 'CP ?= $(value CP)'"\n" >> .config
	printf 'OSNAME ?= $(value OSNAME)'"\n" >> .config
	printf 'CXXNAME ?= $(value CXXNAME)'"\n" >> .config

phf: phf.cc phf.h
	$(CXX) -o $@ $< $(CXXFLAGS) $(CPPFLAGS) -DPHF_MAIN $(LIBS)

$(LIBPHF): phf.cc phf.h
	$(CXX) -o $@ $< $(CXXFLAGS) $(CPPFLAGS) $(LDFLAGS) $(SOFLAGS) $(LIBS)

all: phf $(LIBPHF)

LUAPATH = $(shell env CC="$(CXX)" CPPFLAGS="$(CPPFLAGS)" LDFLAGS="$(LDFLAGS)" mk/luapath -krxm3 $(if $(includedir),$(if $(DESTDIR), -I$(DESTDIR)$(includedir)) -I$(includedir)) -I/usr/include -I/usr/local/include $(if $(DESTDIR),-P$(DESTDIR)$(bindir)) -P$(bindir) -v$(1) $(2))

define LUALIB_BUILD
$(1)/phf.so: phf.cc phf.h
	test "$(1)" = "$$(call LUAPATH, $(1), version)"
	$$(MKDIR) -p $$(@D)
	$$(CXX) -o $$@ $$< $$(CXXFLAGS) $$(CPPFLAGS) $$(call LUAPATH, $(1), cppflags) -DPHF_LUALIB $$(LDFLAGS) $$(LOFLAGS) $(LIBS)

.SECONDARY: all$(1)

lua$(1) all$(1): $(1)/phf.so

ifeq (all, $(filter all, $(or $(MAKECMDGOALS), all)))
ifeq ($(1), $$(call LUAPATH, $(1), version))
all: $(1)/phf.so
endif
endif

endef # LUALIB_BUILD

$(eval $(call LUALIB_BUILD,5.1))

$(eval $(call LUALIB_BUILD,5.2))

$(eval $(call LUALIB_BUILD,5.3))


define LUALIB_INSTALL
ifneq ($(filter install install$(1) uninstall uninstall$(1), $(MAKECMDGOALS)),)
ifeq ($$($(2)),) # define lua5?cpath if empty
$(2)_dyn = $$(call LUAPATH, $(1), cdir)
$(2)_sed = $$(shell printf "$$(luacpath)" | sed -ne 's/[[:digit:]].[[:digit:]]/$(1)/p')
$(2)_lib = $$(libdir)/lua/$(1)

override $(2) = $$(or $$($(2)_dyn), $$($(2)_sed), $$($(2)_lib))
endif

$$($(2))/phf.so: $(1)/phf.so
	$$(MKDIR) -p $$(@D)
	$$(CP) -fp $$< $$@

.SECONDARY: install install$(1)

install install$(1): $$($(2))/phf.so

.PHONY: uninstall uninstall$(1)

uninstall$(1):
	$(RM) -f $$($(2))/phf.so

uninstall: uninstall$(1)

endif # if install or install$(1)
endef # LUALIB_INSTALL

$(eval $(call LUALIB_INSTALL,5.1,lua51cpath))

$(eval $(call LUALIB_INSTALL,5.2,lua52cpath))

$(eval $(call LUALIB_INSTALL,5.3,lua53cpath))


.PHONY: clean distclean clean~

distclean: clean
	$(RM) -f .config

clean:
	$(RM) -f phf
	$(RM) -f $(LIBPHF)
	$(RM) -fr 5.?/
	$(RM) -fr *.dSYM/

clean~:
	find . -name '*~' -exec rm {} +
