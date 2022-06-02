all: server client

server:
	gcc server.c -o server -lpthread

client:
	gcc client.c -o client `pkg-config --cflags --libs gtk+-3.0` -lpthread

clean:
	rm server client tempSong*