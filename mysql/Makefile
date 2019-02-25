# 针对c/c++源码, 源文件后缀 .c .cpp .cc
# .PHONY 伪目标
# TARGET 终极目标
# 自动推导编译: 源目标 -> 目标文件

#[1]
.PHONY: clean install

#[2]
TARGET=mysql.exe
default: $(TARGET)

#[3]
CC_OPTS=
LD_OPTS=-L /usr/lib64/mysql

#[4]
.c.o:
	gcc -g -Wall -c $< $(CC_OPTS)
.cc.o:
	g++ -g -Wall -c $< $(CC_OPTS)
.cpp.o:
	g++ -g -Wall -c $< $(CC_OPTS)

#[5]
LIBS=-lmysqlclient
BASE_OBJS=
MODULE_OBJS=main.o
$(TARGET): $(BASE_OBJS) $(MODULE_OBJS)
	g++ -o $@ $^ $(LD_OPTS) $(LIBS)

#[6]
clean:
	rm -f *.o $(TARGET)

#[7]
install:
# 可以将生成的终极目标及相关的运行依赖库及头文件拷贝到其他路径
