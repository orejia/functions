.PHONY: default clean

#终极目标
TARGETS=a.out
default: $(TARGETS)

#全局编译链接路径
CC_OPTS=
LD_OPTS=

.c.o:
	gcc -Wall -g -c $< $(CC_OPTS) -std=c99

LIBS=-lhiredis
BASE_OBJS=
MODULE_OBJS=main.o
$(TARGETS):$(BASE_OBJS) $(MODULE_OBJS)
	gcc -o $@ $^ $(LD_OPTS) $(LIBS)

clean:
	rm -f *.o
	rm $(TARGETS)


