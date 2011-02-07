CFLAGS= -Wall -g
LIBS= -ltiff -lGLEW
XXD= xxd

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

#-------------------------------------------------------------------------------

OBJS= 	lp-render.o \
	gl-sync.o \
	gl-sphere.o \
	gl-program.o \
	gl-framebuffer.o

GLSL=	lp-circle-vs.glsl \
	lp-circle-fs.glsl \
	lp-sphere-vs.glsl \
	lp-sglobe-fs.glsl \
	lp-spolar-fs.glsl \
	lp-schart-fs.glsl \
	lp-sblend-fs.glsl \
	lp-sfinal-fs.glsl

INCS= $(GLSL:.glsl=.h)

#-------------------------------------------------------------------------------

%.o : %.c
	$(CC) $(CFLAGS) -c $< -o $@

%.h : %.glsl
	$(XXD) -i $< > $@

#-------------------------------------------------------------------------------

$(TARG) : $(OBJS) $(INCS)
	$(CC) $(CFLAGS) $(SHARED) -o $(TARG) $(OBJS) $(LIBS)

clean :
	$(RM) -f $(TARG) $(OBJS) $(INCS)

test : $(TARG)
	./lp-compose driveway.dat

#-------------------------------------------------------------------------------

lp-render.o : lp-render.c lp-render.h $(INCS)

#-------------------------------------------------------------------------------
