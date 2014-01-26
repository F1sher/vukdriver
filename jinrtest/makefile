main.o: jinrtest14.o anizotropy.o time.o expirement.o
	gcc -o main.o jinrtest14.o anizotropy.o time.o expirement.o -lm `pkg-config --cflags --libs gtk+-3.0`

jinrtest14.o: jinrtest14.c
	gcc -c jinrtest14.c -lm `pkg-config --cflags --libs gtk+-3.0`

anizotropy.o: anizotropy.c
	gcc -c anizotropy.c -lm `pkg-config --cflags --libs gtk+-3.0`

time.o: time.c
	gcc -c time.c -lm `pkg-config --cflags --libs gtk+-3.0`
	
expirement.o: expirement.c
	gcc -c expirement.c -lm `pkg-config --cflags --libs gtk+-3.0`
