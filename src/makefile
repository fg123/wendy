CC=gcc

CXXFLAGS= -std=c99
ODIR = ../build
BINDIR = ../bin
SRCDIR = ./
INCDIR = ../headers
CFLAGS=-I $(INCDIR) -lreadline -g -lm -Wall 
CFLAGSNOLINK = -I $(INCDIR) -g

_DEPS = interpreter.h scanner.h token.h error.h stack.h memory.h execpath.h preprocessor.h tests.h macros.h debugger.h ast.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = main.o interpreter.o debugger.o scanner.o stack.o token.o memory.o error.o execpath.o preprocessor.o tests.o ast.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGSNOLINK) $(CXXFLAGS)

main: $(OBJ)
	$(CC) -o $(BINDIR)/wendy $^ $(CFLAGS) $(CXXFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(SRCDIR)/*~ 
