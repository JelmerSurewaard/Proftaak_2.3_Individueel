idf.py clean
@echo off
echo cleaning ccache...
set dr=%cd%
cd %appdata%\.ccache
del /q *.*
forfiles /s /c "cmd /c rmdir /s /q @path"
cd %dr%
echo done,  building...
@echo on
idf.py build