clang++ -std=c++20 -I..\..\opencv\build\include main.cpp -o ValorantModel.exe
rem cl -EHsc main.cpp -I..\..\opencv\build\include -Iinclude
del *.obj