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

OBJS= lp-render.o 

GLSL=	lp-cube-vs.glsl \
	lp-dome-vs.glsl \
	lp-rect-vs.glsl \
	lp-view-vs.glsl \
	lp-accum-data-fs.glsl \
	lp-accum-reso-fs.glsl \
	lp-final-tone-fs.glsl \
	lp-final-data-fs.glsl \
	lp-final-reso-fs.glsl \
	lp-annot-fs.glsl \
	lp-image-vs.glsl \
	lp-image-fs.glsl

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

#-------------------------------------------------------------------------------

lp-render.o : lp-render.c lp-render.h $(INCS)

#-------------------------------------------------------------------------------
