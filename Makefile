CFLAGS := -std=c11
RAYLIB_FLAGS := -lraylib -lGL -lm -lpthread -ldl -lrt -lX11

all: pong

pong: main.c
	$(CC) $(CFLAGS) -o $@ main.c $(RAYLIB_FLAGS)

run: pong
	./pong

clean:
	rm -f pong
