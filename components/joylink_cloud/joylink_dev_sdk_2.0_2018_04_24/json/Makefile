include ../Makefile.rule 
HEADERS = $(wildcard *.h)
ORG_SOURCES = $(wildcard *.c)
OUT_SRC = test.c 

SOURCES=$(filter-out ${OUT_SRC}, ${ORG_SOURCES})
OBJS = $(patsubst %.c, %.o, $(SOURCES))

LIBNAME = $(strip ${shell pwd |xargs basename})

INCLUDES += -I${PROJECT_ROOT_PATH}/joylink
INCLUDES += -I${PROJECT_ROOT_PATH}/extern
STATIC_LIBS += 
LIBS += -lm  

ifeq (${ARCH}, x86)  
all:${OBJS} liba libso
else
all:${OBJS} liba 
endif

.SUFFIXES: .c .o
.c.o:
	${CC} ${CFLAGS} -c $(INCLUDES) ${STATIC_LIBS} $(LIBS) $*.c

liba:${OBJS}
	${AR} -crs lib${LIBNAME}.a ${OBJS}
	${MV} lib${LIBNAME}.a ${TARGET_LIB}

libso:${OBJS}
	${CC}  ${OBJS} -shared -fPIC -o lib${LIBNAME}.so
	${MV} lib${LIBNAME}.so ${TARGET_LIB} 

test:${OBJS}
	${CC} -DLIB_TEST test.c cJSON.c -o $@ ${CFLAGS} ${INCLUDES} ${STATIC_LIBS} ${LIBS} 

clean:
	${RM} *.o *.so *.a test

distclean:clean
	${RM} ./*.a ./*.so ${TARGET_LIB}/lib${LIBNAME}.*

.PHONY:all clean test jt
