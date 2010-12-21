CFLAGS= -Wall -m32

TARG= lp-render.dylib
OBJS= lp-render.o
LIBS= -framework OpenGL

$(TARG) : $(OBJS)
	$(CC) $(CFLAGS) -dynamiclib -o $(TARG) $(OBJS) $(LIBS)

clean :
	$(RM) -f $(TARG) $(OBJS)

