CC = gcc -Wall
CFLAGS = -I.
PROG = mandel

SRCS = math/complex.c mandel.c fraxdraw.c ocl/cachedProgram.c ocl/cachedConstructs.c
LIBS = -lglut -lGLU -lpthread -lOpenCL

all: $(PROG)

$(PROG): $(SRCS)
	$(CC) $(CFLAGS) -o $(PROG) $(SRCS) $(LIBS)

info: cl_info.c
	$(CC) $(CFLAGS) -o cl_info cl_info.c -lOpenCL

test: cl_test.c
	$(CC) $(CFLAGS) -o cl_test cl_test.c -lOpenCL

clean: rm -f $(PROG)
