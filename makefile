CC=gcc
BINDIR = bin
SRCDIR = src
WARNING_FLAGS = -Wall -Wextra -Werror -Wstrict-prototypes
OPT_FLAGS = -fdata-sections -ffunction-sections -Wl,--gc-sections
GIT_COMMIT = $(shell git rev-parse --short HEAD)
CFLAGS = -g -std=gnu99 $(WARNING_FLAGS) $(release) $(FLAGS) $(OPT_FLAGS) -DGIT_COMMIT=\"$(GIT_COMMIT)\"
EXTERNAL_LIBRARIES = -lreadline -lm

_DEPS = *.h
DEPS = $(patsubst %,$(SRCDIR)/%,$(_DEPS))

MAINS = $(SRCDIR)/main.o $(SRCDIR)/vm_main.o
SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(filter-out $(MAINS), $(SRC:%.c=%.o))

all: setup main vm_main libraries test

release: release += -DRELEASE
release: clean all

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

setup:
	mkdir -p $(BINDIR)

vm_main: $(OBJ) $(SRCDIR)/vm_main.o
	$(CC) -o $(BINDIR)/wvm $^ $(EXTERNAL_LIBRARIES) $(CFLAGS) -O3

main: $(OBJ) $(SRCDIR)/main.o
	$(CC) -o $(BINDIR)/wendy $^ $(EXTERNAL_LIBRARIES) $(CFLAGS)
	ln -f $(BINDIR)/wendy $(BINDIR)/windu
	$(MAKE) -C tools/

.PHONY: clean

libraries:
	@bash ./build-libraries.sh

test:
	@bash ./io-test.sh

clean:
	rm -f $(SRCDIR)/*.o *~ core $(SRCDIR)/*~
