# Target library
lib := libfs.a
objs := fs.o disk.o
CC := gcc
CFLAGS := -Wall -Werror -MMD
CFLAGS += -g

## all: $(lib)

## TODO: Phase 1

deps := $(patsubst %.o,%.d,$(objs))
-include $(deps)
DEPFLAGS = -MMD -MF $(@:.o=.d)

ifneq ($(V),1)
Q = @
endif

libfs.a: $(objs)
	@echo "CC $@"
	ar -rcs libfs.a $(objs)

%.o: %.c
	@echo "CC $@"
	$(Q)$(CC) $(CFLAGS) -c -o $@ $< $(DEPFLAGS)

clean:
	$(Q)rm -rf $(lib) $(objs) $(deps)
