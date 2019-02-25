bin_debug := bin_redis_debug
bin_nodebug := bin_redis_nodebug
static_lib := libhashmap.a
dynamic_lib := libhashmap.so
files := $(wildcard *.cpp)
objs := $(files:.cpp=.o)
test_files := example/RedisTest.cpp
test_objs := $(test_files:.cpp=.o)
include_dir :=-I ../../../build-system/3rd-soft/include/
lib_dir :=-L ../../../build-system/3rd-soft/lib/ 
libs := -lhiredis -levent -lpthread -ljsoncpp
CC = gcc
CSRCS = util.c
COBJS = $(CSRCS:.c=.o)

dest_files := $(static_lib) DbConn.h

ifneq "x$(debug)" "xdebug"
cc := g++ -Wall -D_DEBUG_NON -fPIC
bin := $(bin_nodebug)
else
cc := g++ -g -Wall -D_DEBUG_ -fPIC
bin := $(bin_debug)
endif
CP := cp -f

all : static
.PHONY : all

demo : $(bin)
	@$(CP) $(bin) ../test/
.PHONY : demo

check :
	@$(CP) $(dest_files) ../
.PHONY : check

install:all
	@echo install ok!.
.PHONY:install
dynamic:
.PHONY : dynamic

$(bin) : $(objs) $(test_objs)
	$(cc) -o $@ $^ $(lib_dir) $(libs) -g

$(objs) $(test_objs) : %.o : %.cpp
	$(cc) -o $@ -c $< $(include_dir) -g
	
$(COBJS): %.o: %.c 
	$(CC) -o $@ -c $< $(include_dir) -g 
static : $(static_lib) check
.PHONY : static

$(static_lib) : $(objs) $(COBJS)
	@ar rcs $@ $^

clean:
	@rm -f $(bin_nodebug) $(bin_debug) $(objs) $(COBJS)  $(test_objs) \
               $(static_lib) $(dynamic_lib) $(addprefix ../, $(dest_files))
.PHONY : clean

