# Makefile for CygTerm

BINDIR = $(HOME)/bin

CC = gcc
CFLAGS = -D_GNU_SOURCE -O2 -fno-exceptions -I ../../teraterm/common
#CFLAGS = -g -fno-exceptions
LDFLAGS = -mwindows

EXE = cygterm.exe
SRC = \
	$(EXE:.exe=.cc) \
	cygterm_cfg.cc \
	../../teraterm/common/ttlib_static_dirs.cpp \
	../../teraterm/common/asprintf.cpp \
	../../teraterm/common/win32helper.cpp \
	../../teraterm/common/compat_win.cpp \
	../../teraterm/common/ttknownfolders.c \
	../../teraterm/common/dllutil.cpp \
	../../teraterm/common/codeconv.cpp
CFG = $(EXE:.exe=.cfg)
RES = $(EXE:.exe=.res)
ICO = $(EXE:.exe=.ico)
RC  = $(EXE:.exe=.rc)
ARCHIVE = cygterm+.tar.gz

.PHONY: all clean install uninstall

all : $(EXE) $(ARCHIVE)

$(EXE) : $(SRC) $(ICO) $(RC)
  ifeq (0, $(shell nm /usr/lib/crt0.o | grep -c WinMainCRTStartup))
	$(CC) $(CFLAGS) $(LDFLAGS) -DNO_WIN_MAIN -o $(EXE) $(SRC) $(RES)
  else
	$(CC) $(CFLAGS) $(LDFLAGS) -o $(EXE) $(SRC) $(RES) -lole32
  endif
	strip $(EXE)

$(RES): $(RC)
	windres -O coff -o $(RES) $(RC)

$(RC):
	echo 'icon ICON $(ICO)' > $(RC)

clean :
	rm -f $(EXE) $(RC) $(RES) $(ARCHIVE)

install : $(EXE)
	@ install -v $(EXE) $(BINDIR)/$(EXE)
	@ if [ ! -f $(BINDIR)/$(CFG) ]; then \
	    install -v $(CFG) $(BINDIR)/$(CFG) \
	; fi

uninstall :
	rm -f $(BINDIR)/$(EXE)
	rm -f $(BINDIR)/$(CFG)

$(ARCHIVE) : $(SRC) $(ICO) $(CFG) README README-j Makefile
	tar cf - $(SRC) $(ICO) $(CFG) COPYING README README-j Makefile | gzip > $(ARCHIVE)
