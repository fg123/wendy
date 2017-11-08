CC=gcc

CXXFLAGS= -std=c99
ODIR = build
BINDIR = bin
SRCDIR = src
INCDIR = headers
CFLAGS=-I $(INCDIR) -lreadline -g -lm -Wall -Wextra $(release)
CFLAGSNOLINK = -I $(INCDIR) -g $(release)

_DEPS = scanner.h token.h error.h memory.h execpath.h debugger.h ast.h vm.h codegen.h source.h native.h optimizer.h imports.h data.h operators.h global.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = main.o debugger.o scanner.o token.o memory.o error.o execpath.o ast.o codegen.o vm.o global.o source.o native.o optimizer.o imports.o data.o operators.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGSNOLINK) $(CXXFLAGS)

main: $(OBJ)
	$(CC) -o $(BINDIR)/wendy $^ $(CFLAGS) $(CXXFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(SRCDIR)/*~
