CC=gcc

CXXFLAGS= -std=c99
ODIR = ../build
BINDIR = ../bin
SRCDIR = ./
INCDIR = ../headers
CFLAGS=-I $(INCDIR) -lreadline -g -lm -Wall -Wextra
CFLAGSNOLINK = -I $(INCDIR) -g

_DEPS = scanner.h token.h error.h stack.h memory.h execpath.h global.h debugger.h ast.h vm.h codegen.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = main.o debugger.o scanner.o stack.o token.o memory.o error.o execpath.o ast.o codegen.o vm.o global.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGSNOLINK) $(CXXFLAGS)

main: $(OBJ)
	$(CC) -o $(BINDIR)/wendy $^ $(CFLAGS) $(CXXFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(SRCDIR)/*~ 
