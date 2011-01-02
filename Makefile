CFLAGS= -Wall -g
LIBS= -ltiff -lGLEW

ifeq ($(shell uname), Darwin)
	SHARED  = -dynamiclib
	CFLAGS += -I/opt/local/include
        LIBS   += -L/opt/local/lib -framework OpenGL
	TARG    = lp-render.dylib
else
	SHARED  = -shared
	CFLAGS += -fPIC
	LIBS   += -lGLEW -lGL
	TARG    = lp-render.so
endif

OBJS= lp-render.o

#-------------------------------------------------------------------------------

lp-render.o : lp-render.c lp-render.h

#-------------------------------------------------------------------------------

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(TARG) : $(OBJS)
	$(CC) $(CFLAGS) $(SHARED) -o $(TARG) $(OBJS) $(LIBS)

clean :
	$(RM) -f $(TARG) $(OBJS)

#-------------------------------------------------------------------------------
