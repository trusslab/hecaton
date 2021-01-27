cp database/database_header.h Hecaton_source/

python3 scripts/generate_fnc_headers.py bugs/$1/
cp bugs/$1/fncs_header.h Hecaton_source/

path1_dir=$(cd "modified_kernels/pass1_kernel"; printf %s "$PWD")
echo $path1_dir
bug_dir=$(cd "bugs/$1"; printf %s "$PWD")
python3 scripts/path_corrector.py "Hecaton_source/HecatonPass1.cpp" $path1_dir $bug_dir


path2_dir=$(cd "modified_kernels/hecaton_kernel"; printf %s "$PWD")
python3 scripts/path_corrector.py "Hecaton_source/HecatonPass2.cpp" $path2_dir $bug_dir
echo $path2_dir


cd Hecaton_source
ln -s ../compilers/clang/bin/llvm-config llvm-config
source buildHecaton.sh
cd ..

python3 scripts/instrument_makefiles.py bugs/$1/

MYCC_dir=$(cd "compilers/gcc-10/bin"; printf %s "$PWD")
MYCC=$MYCC_dir/gcc
$MYCC -static -lpthread -pthread bugs/$1/poc.c -o bugs/$1/poc.o
