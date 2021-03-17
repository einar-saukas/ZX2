CC = owcc
CFLAGS  = -ox -ob -ol+ -onatx -oh -zp8 -fp6 -g0 -Ot -oe -ot -Wall -xc -s -finline-functions -finline-intrinsics -finline-math -floop-optimize -frerun-optimizer -fno-stack-check -march=i386 -mtune=i686
RM = del
EXTENSION = .exe

all: zx2 dzx2

zx2: zx2.c optimize.c compress.c memory.c zx2.h
	$(CC) $(CFLAGS) -o zx2$(EXTENSION) zx2.c optimize.c compress.c memory.c

dzx2: dzx2.c
	$(CC) $(CFLAGS) -o dzx2$(EXTENSION) dzx2.c

clean:
	$(RM) *.obj
