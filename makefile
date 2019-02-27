CC=gcc
ODIR = build
BINDIR = bin
SRCDIR = src
INCDIR = src
WARNING_FLAGS = -Wall -Wextra -Werror -Wstrict-prototypes
CFLAGS = -g -std=c99 $(WARNING_FLAGS) $(release) $(FLAGS)
EXTERNAL_LIBRARIES = -lreadline -lm

_DEPS = *.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

SRC = $(wildcard $(SRCDIR)/*.c)
OBJ = $(SRC:%.c=%.o)

all: setup main libraries test

release: release += -DRELEASE
release: clean all

$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

setup:
	mkdir -p $(BINDIR)
	mkdir -p $(ODIR)

main: $(OBJ)
	$(CC) -o $(BINDIR)/wendy $^ $(EXTERNAL_LIBRARIES) $(CFLAGS)
	$(MAKE) -C tools/

.PHONY: clean

libraries:
	@bash ./build-libraries.sh

test:
	@bash ./io-test.sh

clean:
	rm -f $(ODIR)/*.o *~ core $(SRCDIR)/*~
