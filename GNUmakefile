name ?= chord

server_name := $(name)
server_src  := daemon.c
server_libs := libdaemon

# A dynamically shared library. We currently don't distribute the
# shared library, this is only used to include the daemon into other
# pieces of code, such as the Android application via JNI. We may
# install the shared library in the future when it has a mature API.
# This variable is supposed to contain the name of the library without
# the suffix (which is platform specific).
lib_name := lib$(name)


# A list of all source code files that form the shared library. This
# also includes the auto-generated source files. Note that the list
# obtained from $(wildcard *.c) needs to be filtered so that
# auto-generated files are removed if they are available from a
# previous compilation run (otherwise they might be included twice).
lib_src := $(filter-out $(server_src), $(wildcard *.c))

all: server $(alldep)


# A list of header files to be installed with the shared library.
lib_hdr  := chord.h

# A list of shared libraries to be discovered via pkg-config.
# Libraries that do not have a pkg-config configuration need to be
# configured manually via CFLAGS and LDFLAGS below. Platform-specific
# libraries can be added to this variable below in platform-specific
# makefile directives (e.g. lua vs lua5.2 for OSX and Linux).
lib_libs := rohc

prefix ?= /usr/local/

CC ?= gcc
AR ?= ar
CFLAGS ?= -Wall -g3
LDFLAGS ?= -lev
PC = pkg-config $(PCFLAGS)

obj_dir := .obj
pic_dir := $(obj_dir)/.pic

alldep = GNUmakefile

# Don't generate dependencies for "clean" targets
nobuild = clean clean-server clean-lib

# Exclude the files matching the following expession from the release tarball.
notar := .git* pkg \#*\# .\#* core $(obj_dir) $(pic_dir) ./$(server_name) *.gz \
	 *.bz2 *.patch *.dsc *.changes *.deb *.tar *.log *.build TODO *.a \
         *.so *.dylib python

CFLAGS += -I. -std=gnu99


# We only want to generate and include dependency files if we're building.
ifeq (,$(MAKECMDGOALS))
    build=1
else
ifneq (,$(filter-out $(nobuild),$(MAKECMDGOALS)))
    build=1
endif
endif


# Normalize the prefix. Make sure it ends with a slash and substitute any
# double slashes with just one.
override prefix := $(subst //,/,$(prefix)/)

ifeq (/usr/local/,$(prefix))
    usr :=
    var_prefix :=
    var := /var/local/
else
    usr := usr/
    var_prefix := $(prefix)
    var := var/
endif

# Configure architecture, operating system, and platform
arch := $(shell $(CC) -E -dM - </dev/null \
	| sed -Ene 's/^\#define[ \t]+__(x86_64|i386|arm)__[ \t]+(.*)$$/\1/p' \
	| tr [A-Z] [a-z])
ifeq (,$(arch))
$(error Unsupported CPU architecture.)
endif

os := $(shell $(CC) -E -dM - </dev/null \
	| sed -Ene 's/^\#define[ \t]+__(linux|APPLE)__[ \t]+(.*)$$/\1/p' \
	| tr [A-Z] [a-z])
ifeq (apple,$(os))
    os:=darwin
endif
ifeq (,$(os))
$(error Unsupported OS type.)
endif

platform := $(shell $(CC) -E -dM - </dev/null \
	| sed -Ene 's/^\#define[ \t]+__(ANDROID|APPLE)__[ \t]+(.*)$$/\1/p' \
	| tr [A-Z] [a-z])
ifeq (apple,$(platform))
    platform:=osx
endif
ifeq (,$(platform))
    platform:=gnu
endif

# Generate version string from history in Git repository and update
# file VERSION if needed. Note that we don't use the variable $(tmp)
# anywhere in this makefile, this is just to execute the script inside
# $(shell). This script only works if we have git and if we are in a
# git repository checkout, hence it is not a good idea to depend on it
# below if we want to be able to compile without git, i.e., from a
# release tarball. The script runs git status before git describe,
# this is to fix an issue in git describe where it incorrectly reports
# the repository as dirty after it has been rsynced from one machine
# to another (this has probably something to do with changes in files
# under .git).
tmp := $(shell \
  [ -x `which git` ] || exit 1; \
  `git status >/dev/null 2>&1`; \
  cur=`git describe --always --tags --dirty="+" 2>/dev/null`; \
  [ -z "$$cur" ] && exit 1; \
  [ -r VERSION ] && old=`cat VERSION`; \
  [ -n "$$old" -a "$$old" = "$$cur" ] && exit 0; \
  echo "$$cur" > VERSION; \
)

# Configure program name, build date and build version
name := $(strip $(name))
date := $(shell date "+%Y-%b-%d %H:%M:%S %Z")
version := $(strip $(shell [ -r VERSION ] && cat VERSION || echo "?"))

lib_name := $(strip $(lib_name))
server_name := $(strip $(server_name))

# Run the rest of the configuration only if we're building
ifeq (1,$(build))

# Enable/disable OS-specific features
ifeq ($(os),linux)
    CFLAGS += -DHAVE_SYS_EPOLL_H
    CFLAGS += -DHAVE_TCP_KEEPCNT -DHAVE_TCP_KEPIDLE -DHAVE_TCP_KEEPINTVL
endif

ifeq ($(platform),gnu)
    CFLAGS += -DHAVE_ENDIAN_H
endif

# Finally, process the libs and flags common for all platforms
CFLAGS += $(shell $(PC) --cflags $(lib_libs))
LDFLAGS += $(shell $(PC) --libs $(lib_libs))
server_LDFLAGS += $(shell $(PC) --libs $(server_libs) $(lib_libs))

CFLAGS += -DLIBDIR='"$(lib_dir)"'
CFLAGS += -DRUNDIR='"$(run_dir)"'
CFLAGS += -DDATADIR='"$(data_dir)"'

CFLAGS += -DARCH='"${arch}"' -D__CPU_${arch}
CFLAGS += -DOS='"${os}"' -D__OS_$(os)
CFLAGS += -DPLATFORM='"${platform}"' -D__PLATFORM_$(platform)
CFLAGS += -DNAME='"$(name)"' -DBUILT='"$(date)"' -DVERSION='"$(version)"'

lib_obj := $(addprefix $(obj_dir)/, $(lib_src:.c=.o))
pic_obj := $(addprefix $(pic_dir)/, $(lib_src:.c=.o))

server_obj := $(addprefix $(obj_dir)/, $(server_src:.c=.o))

# The list of all the object files, for all build targets.
obj := $(lib_obj) $(pic_obj) $(server_obj)

# The list of all dependency files to be included at the end of the
# Makefile
deps := $(obj:.o=.d)

# Make sure all object directories exist before we start building.
tmp := $(shell mkdir -p $(sort $(dir $(obj))))

endif # ifeq (1,$(build))

server: $(server_name) $(alldep)

lib: $(lib_name).so $(lib_name).a $(alldep)

# This object file has one of the variables initialized to the version
# of the project, thus it needs to depend on the file VERSION so that
# it gets recompiled whenever the version string changes. This rule is
# needed only if you want to make sure that the version string in the
# binary is up-to-date with the version obtained from git.
$(obj_dir)/chord.o $(pic_dir)/chord.o: VERSION

VERSION:

define cc-cmd
@test -d `dirname "$@"` || mkdir -p `dirname "$@"`
$(CC) -MMD -MP -MT "$@ $(@:.o=.d)" -c $(CFLAGS) $(1) -o $@ $<
endef

$(obj_dir)/%.o: %.c $(alldep)
	$(call cc-cmd)

$(pic_dir)/%.o: %.c $(alldep)
	$(call cc-cmd,-fPIC -DPIC)


$(lib_name).a: $(lib_obj) $(alldep)
	$(AR) rcs $@ $(lib_obj)


# Shared libraries on Android/Linux (without version)
$(lib_name).so: $(pic_obj) $(alldep)
	$(CC) $(CFLAGS) -shared -Wl,-soname,$@ -o $@ $(pic_obj) $(LDFLAGS)

$(server_name): $(server_obj) $(lib_name).a $(alldep)
	$(CC) $(CFLAGS) -o $@ $(server_obj) $(lib_name).a $(LDFLAGS) $(server_LDFLAGS)


$(DESTDIR)$(prefix)$(usr)sbin \
$(DESTDIR)$(prefix)$(usr)lib \
$(DESTDIR)$(prefix)$(usr)lib/pkgconfig \
$(DESTDIR)$(lib_dir) \
$(DESTDIR)$(prefix)$(usr)include \
$(DESTDIR)$(prefix)$(usr)share/doc/$(name):
	install -d "$@"

install: install-server $(alldep)

install-server: $(DESTDIR)$(prefix)$(usr)sbin VERSION README $(alldep) \
	$(DESTDIR)$(prefix)$(usr)share/doc/$(name) $(server_name) \
	install -s $(server_name) "$(DESTDIR)$(prefix)$(usr)sbin/$(server_name)"
	install -m 644 README VERSION \
	    "$(DESTDIR)$(prefix)$(usr)share/doc/$(name)"

install-hdr: $(DESTDIR)$(prefix)$(usr)include $(lib_hdr) $(alldep)
	install -m 444 $(lib_hdr) "$(DESTDIR)$(prefix)$(usr)include"

install-alib: $(DESTDIR)$(prefix)$(usr)lib $(lib_name).a $(alldep)
	install $(lib_name).a "$(DESTDIR)$(prefix)$(usr)lib"

install-pc: $(DESTDIR)$(prefix)$(usr)lib/pkgconfig $(alldep)
	@echo 'prefix=$(prefix)$(usr)' > $</$(lib_name).pc
	@echo 'exec_prefix=$${prefix}' >> $</$(lib_name).pc
	@echo 'libdir=$${exec_prefix}/lib' >> $</$(lib_name).pc
	@echo 'includedir=$${prefix}/include' >> $</$(lib_name).pc
	@echo '' >> $</$(lib_name).pc
	@echo 'Name: $(lib_name)' >> $</$(lib_name).pc
	@echo 'Description: Smart Object Implementation' >> $</$(lib_name).pc
	@echo 'Version: $(version)' >> $</$(lib_name).pc
	@echo 'Requires.private: $(lib_libs)' >> $</$(lib_name).pc
	@echo 'Libs: -L$${libdir} -l$(name)' >> $</$(lib_name).pc
	@echo 'Libs.private: $(LDFLAGS)' >> $</$(lib_name).pc
	@echo 'Cflags: -I$${includedir}' >> $</$(lib_name).pc


install-lib: install-hdr install-alib install-pc $(lib_name).so $(alldep)
	install $(lib_name).so "$(DESTDIR)$(prefix)$(usr)lib"


.PHONY: clean-server
clean-server:
	rm -f $(server_name)


clean-lib:
	rm -f $(lib_name).*


.PHONY: clean
clean: clean-server clean-lib
	rm -rf "$(obj_dir)" "$(pic_dir)"


ifeq (1,$(build))
-include $(deps)
endif
