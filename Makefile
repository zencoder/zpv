LIBS=
UNAME := $(shell uname)

ifeq ($(UNAME), Linux)
	LIBS += -lrt
endif

all:
	gcc -O2 main.c $(LIBS) -o zpv

clean:
	rm zpv

install: zpv
	cp zpv /usr/local/bin/

uninstall:
	rm /usr/local/bin/zpv
