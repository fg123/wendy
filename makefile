CC=gcc
BINDIR = bin
SRCDIR = src
WARNING_FLAGS = -Wall -Wextra -Werror -Wstrict-prototypes
GIT_COMMIT = $(shell git rev-parse --short HEAD)
CFLAGS = -g -std=c99 $(WARNING_FLAGS) $(release) $(FLAGS) -DGIT_COMMIT=\"$(GIT_COMMIT)\"
EXTERNAL_LIBRARIES = -lreadline -lm

_DEPS = *.h
DEPS = $(patsubst %,$(SRCDIR)/%,$(_DEPS))

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:%.c=%.o)

all: setup main libraries test

release: release += -DRELEASE
release: clean all

$(SRCDIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

setup:
	mkdir -p $(BINDIR)

main: $(OBJ)
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
