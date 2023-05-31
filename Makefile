all:app
obj=camera.o freetype.o jpg.o http_server.o
CC:=gcc
VPATH=./freetype ./jpg ./camera #指定依赖文件搜索路径
CFLAGS=-I/home/toyz/src_pack/freetype-2.4.10/_install/include #指定freetype头文件路径
CFLAGS+=-I/home/toyz/src_pack/freetype-2.4.10/_install/include/freetype2 #指定freetype头文件路径
CFLAGS+=-lfreetype -ljpeg -pthread #指定动态库

app:$(obj)
	@$(CC) $^ -o $@ $(CFLAGS)

.PHONY:clean
clean:
	@rm app *.o -f
