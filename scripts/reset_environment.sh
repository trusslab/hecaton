rm Hecaton_source/fncs_header.h
rm Hecaton_source/database_header.h


cd modified_kernels/hecaton_kernel
git reset --hard
git checkout Hecaton
make clean
cd ../..


cd modified_kernels/pass1_kernel
git reset --hard
git checkout Hecaton
make clean
cd ../..


cd modified_kernels/baseline_kernel
git reset --hard
git checkout Hecaton
make clean
cd ../..

git checkout Hecaton_source/HecatonPass1.cpp
git checkout Hecaton_source/HecatonPass2.cpp

rm Hecaton_source/llvm-config

rm Hecaton_source/*.so





