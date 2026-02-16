.PHONY: default
default: all

SOURCES += from_map16.cpp
SOURCES += to_map16.cpp
SOURCES += human_readable_map16.cpp
SOURCES += c_api.cpp

ifeq ($(PLATFORM),)
  ifeq ($(OS),Windows_NT)
    PLATFORM = windows
    ARCH ?= $(PROCESSOR_ARCHITECTURE)
  else
    UNAME = $(shell uname -s)
    ifeq ($(UNAME),Darwin)
      PLATFORM = macos
      ARCH ?= $(shell uname -m)
    else ifeq ($(UNAME),Linux)
      PLATFORM = linux
      ARCH ?= $(shell uname -m)
    else
      PLATFORM = unknown
    endif
  endif
endif

ifeq ($(PLATFORM),unknown)
  $(error unknown platform, please specify PLATFORM= windows, macos, or linux)
else
  $(info ==========  building for $(PLATFORM)-$(ARCH) ============)
endif

ifeq ($(PLATFORM),windows)
  LIBEXT = dll
  STATIC_LIBEXT = lib
endif
ifeq ($(PLATFORM),macos)
  LIBEXT = dylib
  STATIC_LIBEXT = a
endif
ifeq ($(PLATFORM),linux)
  LIBEXT = so
  STATIC_LIBEXT = a
endif

FLAGS += $(CPPFLAGS)
FLAGS += -Wall
FLAGS += --std=c++17

ifeq ($(STATIC),1)
  LIB = human_readable_map16.$(STATIC_LIBEXT)
  $(LIB): $(SOURCES)
	$(CXX) $(FLAGS) -c $(SOURCES)
	$(AR) rcs $@ *.o
else
  LIB = human_readable_map16.$(LIBEXT)
  ifeq ($(PLATFORM),windows)
    DYNFLAG = -shared
  endif
  ifeq ($(PLATFORM),macos)
    DYNFLAG = -dynamiclib
  endif
  ifeq ($(PLATFORM),linux)
    DYNFLAG = -shared -fPIC
  endif
  $(LIB): $(SOURCES)
	$(CXX) $(DYNFLAG) $(FLAGS) $(SOURCES) -o $@
endif

.PHONY: all
all: $(LIB)

.PHONY: clean
clean:
	rm -f $(LIB)
