CC=gcc
BINDIR = bin
SRCDIR = src
WARNING_FLAGS = -Wall -Wextra -Werror -Wstrict-prototypes
OPT_FLAGS = -fdata-sections -ffunction-sections -Wl,--gc-sections
GIT_COMMIT = $(shell git rev-parse --short HEAD)
CFLAGS = -g -std=gnu99 $(WARNING_FLAGS) $(release) $(FLAGS) $(OPT_FLAGS) -DGIT_COMMIT=\"$(GIT_COMMIT)\"
LINK_FLAGS = -lreadline -lm

_DEPS = *.h
DEPS = $(patsubst %,$(SRCDIR)/%,$(_DEPS))

MAINS = $(SRCDIR)/main.o $(SRCDIR)/vm_main.o
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(filter-out $(MAINS), $(SRC:%.c=%.o))

all: test

release: release += -DRELEASE
release: LINK_FLAGS += -O3
release: | clean all

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

setup:
	mkdir -p $(BINDIR)

vm_main: $(OBJ) $(SRCDIR)/vm_main.o setup
	$(CC) -o $(BINDIR)/wvm $(OBJ) $(SRCDIR)/vm_main.o $(LINK_FLAGS) $(CFLAGS)

main: $(OBJ) $(SRCDIR)/main.o setup
	$(CC) -o $(BINDIR)/wendy $(OBJ) $(SRCDIR)/main.o $(LINK_FLAGS) $(CFLAGS)
	ln -f $(BINDIR)/wendy $(BINDIR)/windu
	$(MAKE) -C tools/

library: $(OBJ) setup $(DEPS)
	ar rvs $(BINDIR)/wendy.a $(OBJ)
	
.PHONY: clean

libraries: vm_main main
	@bash ./build-libraries.sh

test: libraries
	@bash ./io-test.sh

clean:
	rm -f $(SRCDIR)/*.o *~ core $(SRCDIR)/*~
