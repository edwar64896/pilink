IDIR =../include 
CC=gcc
CFLAGS=-I$(IDIR) -I/opt/vc/include -g  

ODIR=obj
LDIR =../lib 

LIBS=-lm  -lrt -lpthread -lserialport -luev `simple2d --libs` 

_DEPS =
DEPS = $(patsubst %,$(IDIR)/%,$(_DEPS))

_OBJ = sd788t.o pq.o clink.o
OBJ = $(patsubst %,$(ODIR)/%,$(_OBJ))


$(ODIR)/%.o: %.c $(DEPS)
	$(CC) -c -o $@ $< $(CFLAGS)

788t: $(_OBJ)
	$(CC) -o $@ $^ $(CFLAGS) $(LIBS)

.PHONY: clean

clean:
	rm -f $(ODIR)/*.o *~ core $(INCDIR)/*~  timer 788t *.o

all: 788t 
