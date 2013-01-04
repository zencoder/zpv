OS := $(shell uname)
ifeq ($(OS),Linux)
EV_BACKEND += -DEV_USE_EPOLL
endif
ifeq ($(OS),Darwin)
EV_BACKEND += -DEV_USE_KQUEUE
endif

all:
	gcc -O3 $(EV_BACKEND) main.c -o zpv

clean:
	rm zpv

install: zegmenter
	cp zpv /usr/local/bin/

uninstall:
	rm /usr/local/bin/zpv


