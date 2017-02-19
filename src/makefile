CC=gcc

CXXFLAGS= -std=c99
ODIR = ../build
BINDIR = ../bin
SRCDIR = ./
INCDIR = ../headers
CFLAGS=-I $(INCDIR) -lreadline -g 

_DEPS = interpreter.h scanner.h token.h error.h stack.h memory.h
DEPS = $(patsubst %,$(INCDIR)/%,$(_DEPS))

_OBJ = main.o interpreter.o scanner.o stack.o token.o memory.o error.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: $(SRCDIR)/%.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS) $(CXXFLAGS)

main: $(OBJ)
	$(CC) -o $(BINDIR)/wendy $^ $(CFLAGS) $(CXXFLAGS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(SRCDIR)/*~ 
