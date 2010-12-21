CFLAGS= -Wall
LIBS=

ifeq ($(shell uname), Darwin)
	CFLAGS += -m32
	SHARED  = -dynamiclib
        LIBS   += -framework OpenGL
	TARG    = lp-render.dylib
else
	CFLAGS += -fPIC
	SHARED  = -shared
	LIBS   += -lGLEW -lGL
	TARG    = lp-render.so
endif


OBJS= lp-render.o

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARG) : $(OBJS)
	$(CC) $(CFLAGS) $(SHARED) -o $(TARG) $(OBJS) $(LIBS)

clean :
	$(RM) -f $(TARG) $(OBJS)

