#
# Copyright (C) 2022 Free Software Foundation, Inc.

#   This program is free software: you can redistribute it and/or modify
#   it under the terms of the GNU General Public License as published by
#   the Free Software Foundation, either version 3 of the License, or
#   (at your option) any later version.
#
#   This program is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#   GNU General Public License for more details.
#
#   You should have received a copy of the GNU General Public License
#   along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

name = cdstack
prefix = /usr
exec_prefix = 
libdir = ${exec_prefix}/lib
includedir = ${prefix}/include
headersdir = ${includedir}/bash
loadablesdir = ${libdir}/bash

CC = gcc
RM = rm -f
MKDIR = mkdir -p
SHELL = /bin/sh

DEFS = -DHAVE_CONFIG_H
LOCAL_DEFS = -DSHELL -D_GNU_SOURCE
LOCAL_CFLAGS = 
CPPFLAGS = -Wdate-time -D_FORTIFY_SOURCE=2
CFLAGS = -std=gnu23 -O3 -DNDEBUG -pedantic -pipe -mtune=native -march=native \
		-fomit-frame-pointer -Werror=implicit-function-declaration -fstack-protector-strong \
		-fstack-clash-protection -Wformat -Werror=format-security -fcf-protection -Wall

STYLE_CFLAGS = 

CCFLAGS = $(DEFS) $(LOCAL_DEFS) $(LOCAL_CFLAGS) $(CPPFLAGS) $(CFLAGS) $(STYLE_CFLAGS)

INSTALL_MODE = -m 0755
INSTALL_FLAGS = -c
INSTALL = /usr/bin/install $(INSTALL_MODE) $(INSTALL_FLAGS)

SHOBJ_CC = $(CC)
SHOBJ_CFLAGS = -fPIC
SHOBJ_LD = $(SHOBJ_CC)
SHOBJ_LDFLAGS = -shared -Wl,-soname,$@ -Wl,-z,relro -Wl,-z,now
SHOBJ_XLDFLAGS = 
SHOBJ_LIBS = 

INC = -I${includedir} -I${headersdir} -I${headersdir}/include -I${headersdir}/builtins -I${loadablesdir}

.c.o:
	$(SHOBJ_CC) $(SHOBJ_CFLAGS) $(CCFLAGS) $(INC) -c -o $@ $<

all: $(name)

$(name): $(name).o
	$(SHOBJ_LD) $(SHOBJ_LDFLAGS) $(SHOBJ_XLDFLAGS) -o $@ $(name).o $(SHOBJ_LIBS)

$(name).o: $(name).c

debug: CFLAGS = -std=gnu23 -pedantic -g3 -ggdb -O0 -DDEBUG -Wall \
		-fstack-protector-strong -Werror=implicit-function-declaration -Wconversion -Wextra \
		-Werror=format-security -Wformat -Wformat-signedness -Wmissing-include-dirs \
		-Wnested-externs -Wold-style-definition -Wshadow -Wstrict-prototypes -Wwrite-strings

debug: $(name)

clean:
	$(RM) $(name) *.o


install: | installdir
	$(INSTALL) $(name) ${loadablesdir}/$(name)


installdir:
	$(MKDIR) ${loadablesdir}

uninstall:
	$(RM) ${loadablesdir}/$(name)
