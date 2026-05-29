#递归将本文件夹下所有cpp
file(GLOB_RECURSE APP ./  *.cpp *.cc)

add_library(app ${APP} )
