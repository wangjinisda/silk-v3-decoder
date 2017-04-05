

# this is my sample dll for c# import
# use mingw32-gcc will help build DLL or EXE that is win32 compatible

# mingw32-gcc -Wall -shared mingw_dll.c -o mingw_dll.dll

# mingw32-gcc use_mingw_dll.c mingw_dll.dll -o use_mingw_dll