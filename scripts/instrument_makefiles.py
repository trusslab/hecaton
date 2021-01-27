import sys 
import os
if ( len(sys.argv) !=2 ):
    print("usage: instrument_makefiles.py bugdir ")

f = open ( sys.argv[1] + 'directories.txt' )
lines = f.readlines();

for line in lines:
    dir_name = line.rstrip('\n')
    file_name1 = 'modified_kernels/baseline_kernel/'+ line.rstrip('\n') +'Makefile'
    print( 'instrumenting: ' + file_name1)
    file1 = open(file_name1)
    file1_lines = file1.readlines()
    file1.close()
    hecaton_path = os.path.abspath('Hecaton_source')
    inst1 = '##Hecaton start\n';
    inst2 = 'ccflags-y\t+= -Xclang -load -Xclang ' +  hecaton_path +'/hecaton_pass1.so\n';
    inst3 = 'ccflags-y\t+= -Xclang -plugin -Xclang hecaton-pass1\n';
    inst4 = '##Hecaton end\n';
    file1_lines = [ inst1,inst2,inst3,inst4, *file1_lines ]
    file1 = open(file_name1,'w')
    for file1_line in file1_lines:
        file1.write(file1_line)
    file1.close()    

    file_name2 = 'modified_kernels/pass1_kernel/'+ line.rstrip('\n') +'Makefile'
    print( 'instrumenting: ' + file_name2)
    file2 = open(file_name2)
    file2_lines = file2.readlines()
    file2.close()
    hecaton_path = os.path.abspath('Hecaton_source')
    inst1 = '##Hecaton start\n';
    inst2 = 'ccflags-y\t+= -Xclang -load -Xclang ' +  hecaton_path +'/hecaton_pass2.so\n';
    inst3 = 'ccflags-y\t+= -Xclang -plugin -Xclang hecaton-pass2\n';
    inst4 = '##Hecaton end\n';
    file2_lines = [ inst1,inst2,inst3,inst4, *file2_lines ]
    file2 = open(file_name2,'w')
    for file2_line in file2_lines:
        file2.write(file2_line)
    file2.close()    

file_name3 = 'modified_kernels/baseline_kernel/build_clang.sh'
file3 = open(file_name3, 'a+')
file_name4 = 'modified_kernels/pass1_kernel/build_clang.sh'
file4 = open(file_name4, 'a+')
for line in lines:
    dir1 = line.rstrip('\n');
    str1 = '\nmake CC=$MYCC -j30 -k '+ dir1  +' 2>&1 | tee build.log\n'
    file3.write(str1)
    file4.write(str1)

