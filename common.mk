OBJECTS := $(patsubst %.c,obj/%.o,$(SOURCES))

RANLIB?=ranlib
AR?=ar

all: $(TARGET)

vsock_cli vsock_srv: $(OBJECTS)
	$(CC) $^ -o $@ $(LDFLAGS)

%.a: $(OBJECTS)
	$(AR) rc $@ $^
	$(RANLIB) $@

obj:
	mkdir $@

obj/%.o: %.c obj
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	$(RM) $(OBJECTS) $(TARGET)
	if test -e obj; then rmdir obj; fi

.PHONY: all clean
