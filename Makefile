all:
	gcc -O2 main.c -o zpv

clean:
	rm zpv

install: zegmenter
	cp zpv /usr/local/bin/

uninstall:
	rm /usr/local/bin/zpv


