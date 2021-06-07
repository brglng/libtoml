CONFIG = config.mk
include $(CONFIG)

all: static_lib dynamic_lib

compile:
	$(CC) $(CFLAGS) -c toml.c

static_lib: compile
	$(AR) libtoml.a toml.o

dynamic_lib:
	$(CC) $(CFLAGS) -fPIC -shared -o libtoml.so toml.c

clean:
	${RM} toml.o; ${RM} libtoml.a; ${RM} libtoml.so

install: all
	cp libtoml.* $(PREFIX)/lib/; cp toml.h $(PREFIX)/include/

uninstall:
	cd $(PREFIX); ${RM} lib/libtoml.a lib/libtoml.so; ${RM} include/toml.h
