CC=gcc

CXXFLAGS= -std=c99
ODIR = build
BINDIR = bin
SRCDIR = src
INCDIR = headers
CFLAGS=-I $(INCDIR) -lreadline -g -lm -Wall -Wextra
CFLAGSNOLINK = -I $(INCDIR) -g

_DEPS = scanner.h token.h error.h memory.h execpath.h global.h debugger.h ast.h vm.h codegen.h source.h native.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = main.o debugger.o scanner.o token.o memory.o error.o execpath.o ast.o codegen.o vm.o global.o source.o native.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGSNOLINK) $(CXXFLAGS)

main: $(OBJ)
	$(CC) -o $(BINDIR)/wendy $^ $(CFLAGS) $(CXXFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(SRCDIR)/*~ 
