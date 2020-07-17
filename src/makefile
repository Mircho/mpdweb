LOCAL_CONFIG ?= 1

TARGET	 =	mpdweb

DEFS	= -DLOCAL_CONFIG=$(LOCAL_CONFIG)

LDFLAGS	 +=	-lpthread -ljansson -lmpdclient -luci -Wl,-rpath
CFLAGS	 +=	-Wall -g $(DEFS)

SRCDIR	 =
ODIR	 = ./obj/
OUTDIR	 = ../

DEPS	 =  config.h mpdobserver.h utils.h
_CSRC	 =	main.c mongoose.c mpdobserver.c config.c utils.c
CSRC	 =  $(patsubst %,$(SRCDIR)%,$(_CSRC))
_OBJ	 =	$(CSRC:.c=.o)
OBJ		 =  $(patsubst %,$(ODIR)%,$(_OBJ))


.PHONY: clean dirs

$(ODIR)%.o:	%.c $(DEPS)
		$(CC) -c -o $@ $< $(CFLAGS) $(LDFLAGS)

$(TARGET): $(OBJ)
		$(CC) -o $(OUTDIR)$@ $^ $(LDFLAGS)

all: 	$(TARGET)


dirs:
		mkdir -p obj

clean:
		rm -rf $(ODIR)*.o $(OUTDIR)$(TARGET)
