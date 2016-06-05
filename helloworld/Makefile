
bin = helloworld


all: $(bin)

$(bin): $(bin).c
	gcc -Wall $(CFLAGS) $(CPPFLAGS) `pkg-config --cflags gtk+-2.0` $(LDFLAGS) $< -o $@ `pkg-config --libs gtk+-2.0`

clean:
	rm -f $(bin)

