CC=gcc
ODIR = build
BINDIR = bin
SRCDIR = src
INCDIR = src
WARNING_FLAGS = -Wall -Wextra -Werror -Wstrict-prototypes
CFLAGS = -g -std=c99 $(WARNING_FLAGS) $(release)
EXTERNAL_LIBRARIES = -lreadline -lm

_DEPS = *.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = main.o debugger.o scanner.o token.o memory.o error.o execpath.o ast.o \
	codegen.o vm.o global.o source.o native.o optimizer.o imports.o data.o \
	operators.o dependencies.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))

all: main libraries test

release: release += -DRELEASE
release: all

$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

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
