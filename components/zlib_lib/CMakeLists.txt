#该文件由于导入zlib库，不是zlib源项目的部分
set(COMPONENT_ADD_INCLUDEDIRS ./zlib)
set(COMPONENT_SRCS
./zlib/adler32.c
./zlib/crc32.c
./zlib/deflate.c
./zlib/zutil.c
./zlib/trees.c 
./zlib/infback.c
./zlib/inffast.c
./zlib/inflate.c 
./zlib/inftrees.c
./zlib/uncompr.c
)
register_component()


