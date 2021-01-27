import sys 
import os
if ( len(sys.argv) !=4 ):
    print("usage: path_corrector.py Hecaton_source path_kernel bugdir ")

print(sys.argv[1])
f = open ( sys.argv[1]);
lines = f.readlines();
f.close()

new_lines = []

for line in lines:
    line2 = line;
    if('XXXPATHXXX' in line):
        line2 = line.replace('XXXPATHXXX',sys.argv[2]+'/')
        #print(line2)
    if('XXXSCOREPATHXXX' in line):
        line2 = line.replace('XXXSCOREPATHXXX',sys.argv[3]+'/scores.txt')
        #print(line2)
    new_lines.append(line2)    


f = open ( sys.argv[1],'w');
for line in new_lines:
    f.write(line)
f.close()    

    
