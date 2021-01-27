absolute=$(cd "../compilers/clang/include/"; printf %s "$PWD")
echo $absolute

../compilers/clang/bin/clang++ -v -I$absolute -std=c++14 HecatonDatabase.cpp \
 $(./llvm-config --cxxflags --ldflags) \
  -fPIC -o hecaton_database.so -shared -Wl,-undefined,dynamic_lookup

../compilers/clang/bin/clang++ -v -I$absolute -std=c++14 HecatonPass1.cpp \
 $(./llvm-config --cxxflags --ldflags) \
  -fPIC -o hecaton_pass1.so -shared -Wl,-undefined,dynamic_lookup

../compilers/clang/bin/clang++ -v -I$absolute -std=c++14 HecatonPass2.cpp \
 $(./llvm-config --cxxflags --ldflags) \
  -fPIC -o hecaton_pass2.so -shared -Wl,-undefined,dynamic_lookup

