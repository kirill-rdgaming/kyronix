CFLAGS := -nostdlib -static -m64

.PHONY: all clean

all: init hello

%: %.S
	gcc $(CFLAGS) -o $@ $<

clean:
	rm -f init hello
