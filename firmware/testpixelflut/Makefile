all : testpf

CFLAGS:=-I../emu/rawdraw -g
LDFLAGS:=-lX11 -lXext -lXtst

testpf : testpf.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf testpf

