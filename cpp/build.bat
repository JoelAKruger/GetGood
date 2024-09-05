rem clang -I..\..\opencv\build\include main.cpp -o ValorantModel.exe
rem cl -EHsc -Zi -I..\..\opencv\build\include -Iinclude main.cpp
cl -EHsc -Zi -O2 -I..\..\opencv\build\include -Iinclude ScreenCapture.cpp
del *.obj