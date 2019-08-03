.PHONY: clean
objects=test.o leptjson.o
jsontest:$(objects)
	gcc -o jsontest $(objects) -g
leptjson.o:leptjson.h leptjson.c
	gcc -c leptjson.c -g
test.o:leptjson.h test.c
	gcc -c test.c -g
clean:
	rm *.o jsontest