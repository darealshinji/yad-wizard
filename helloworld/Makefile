ifeq ($(shell uname -m),x86_64)
	ARCH = x86_64
else
	ARCH = x86
endif

bin = helloworld.$(ARCH)


all: $(bin)

$(bin): helloworld.c
	gcc -Wall $(CFLAGS) $(CPPFLAGS) `pkg-config --cflags gtk+-2.0` $(LDFLAGS) $< -o $@ `pkg-config --libs gtk+-2.0`

clean:
	rm -f $(bin)

