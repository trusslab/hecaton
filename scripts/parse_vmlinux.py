import sys 

STATE_RESET = 0
STATE_RBP = 1
STATE_R15 = 15
STATE_R14 = 14
STATE_R13 = 13
STATE_R12 = 12
STATE_RBX = 11

class FuncClass: 
    def __init__(self):
        self.base_addr = ''
        self.supported = 0
        self.base_line = 0
        self.end_addr = ''
        self.end_line = 0
        self.base_addr_cold = '0'
        self.base_line_cold = 0
        self.has_cold = 0
        self.end_addr_cold = '0'
        self.end_line_cold = 0
        self.rbx = 100
        self.r12 = 100
        self.r13 = 100
        self.r14 = 100
        self.r15 = 100
        return

file_name = ''
if (len(sys.argv) != 4): 
    print ('useage: python parse_vmlinux.py vmlinux.txt supporrted.txt hecaton.h')
    sys.exit(1)
else:
    vmlinux_str = sys.argv[1]
    supp_str = sys.argv[2]
    out_str = sys.argv[3]
   	

try:
    vmlinux_file = open( vmlinux_str, 'r')
except IOError:
    print ('cannot open' , source_name )
    sys.exit(1)

try:
    out_file = open( out_str, 'w')
except IOError:
    print ('cannot open' , out_str)
    sys.exit(1)



try:
    supp_file = open( supp_str, 'r')
except IOError:
    print ('cannot open' , supp_str)
    sys.exit(1)

lines = vmlinux_file.readlines()
supp_lines = supp_file.readlines()

functions = []
#for line in lines:
#	if ">:" in line:
#		x = line.split()
#		print x[1]
#

i =-1
array_size = 0
old_percentage = 0
cold_array_size = 0
print ( 'Finding functions bounderies' , file=sys.stderr)
while (i<len(lines)-1):
    i += 1    
    line = lines[i]
    percentage = (i *100) // len(lines) 
    if percentage != old_percentage :
        print ( percentage , file=sys.stderr)
  #  print ( percentage , old_percentage , i, file=sys.stderr)
    old_percentage = percentage    
    func = FuncClass()
    if ">:" in line:
        x = line.split()
        name = x[1]
        addr = x[0]
        name = name[1:(len(name)-2)]
        #print name,addr
        if "cold" in name:
            continue
        func.name = name
        func.base_addr = addr
        func.base_line = i
        found_end = 0 
        while (found_end == 0 and i<len(lines)-1):
            i += 1
            line2 = lines[i]
            if  ".cold" in line2:
                func.has_cold = 1
            if line2 =='\n':
                found_end = 1
                func.end_line = i -1
                line3 = lines[i-1]
                y = line3.split()
                end_addr = y[0]
                end_addr = end_addr.rstrip(':')
                break
        func.end_addr = end_addr
        if(end_addr == '...'):
            continue
        functions.append(func)

print ( 'Finding cold part of functions bounderies' , file=sys.stderr)
counter = 0
for f in functions:
    counter += 1
    percentage = (counter *100) // len(functions) 
    if percentage != old_percentage :
        print ( percentage , file=sys.stderr)
    old_percentage = percentage    

    if( f.has_cold == 0):
        continue
    name_cold = f.name + '.cold'
    i = f.end_line
    found_cold = 0
    while( found_cold == 0 and i < len(lines)-1):
        i += 1
        line = lines[i]
        if ">:" in line:
            #print ( name_cold, '\t' ,line , file=sys.stderr)
            x = line.split()
            name = x[1]
            addr = x[0]
            name = name[ 1 : (len(name)-2) ]
            if (name == name_cold):
                #print ( "OK" , file=sys.stderr)
                f.base_line_cold = i
                f.base_addr_cold = addr
                found_end = 0 
                while (found_end == 0 and i<len(lines)-1):
                    i += 1
                    line2 = lines[i]
                    if line2 =='\n':
                        found_end = 1
                        f.end_line_cold = i -1
                        line3 = lines[i-1]
                        y = line3.split()
                        f.end_addr_cold = y[0].rstrip(':')
                        break
                break
                
print ( 'Finding callee saved registers of functions' , file=sys.stderr)
counter = 0
for f in functions:
    state = STATE_RESET
    offset = 0
  #  print('\n', f.name)
    counter += 1
    percentage = (counter *100) // len(functions) 
    if percentage != old_percentage :
        print ( percentage , file=sys.stderr)
    old_percentage = percentage    
    ln = f.base_line
    while (ln < f.end_line):
        line = lines[ln]
        if( '\tpush ' in line):
            words = line.split()
            reg = words[-1]
           # print(ln,'::\t',reg)
            if ( state == STATE_RBX ):
                break
            if ( state == STATE_RESET):
                if( reg == '%rbp'):
                   # print('rbp found go to STATE_RBP')
                    state = STATE_RBP
                    offset -= 8
            if ( state == STATE_RBP ):
                if ( reg == '%r15'):
                   # print('r15 found go to STATE_R15 , offset= ', offset)
                    f.r15 = offset
                    state = STATE_R15
                    offset -= 8
                if ( reg == '%r14'):
                   # print('r14 found go to STATE_R14 , offset= ', offset)
                    f.r14 = offset
                    state = STATE_R14
                    offset -= 8
                if ( reg == '%r13'):
                   # print('r13 found go to STATE_R13 , offset= ', offset)
                    f.r13 = offset
                    state = STATE_R13
                    offset -= 8
                if ( reg == '%r12'):
                   # print('r12 found go to STATE_R12 , offset= ', offset)
                    f.r12 = offset
                    state = STATE_R12
                    offset -= 8
                if ( reg == '%rbx'):
                   # print('rbx found go to STATE_RBX , offset= ', offset)
                    f.rbx = offset
                    state = STATE_RBX
                    offset -= 8
            if ( state == STATE_R15 ):
                if ( reg == '%r14'):
                    #print('r14 found go to STATE_R14 , offset= ', offset)
                    f.r14 = offset
                    state = STATE_R14
                    offset -= 8
                if ( reg == '%r13'):
                   # print('r13 found go to STATE_R13 , offset= ', offset)
                    f.r13 = offset
                    state = STATE_R13
                    offset -= 8
                if ( reg == '%r12'):
                   # print('r12 found go to STATE_R12 , offset= ', offset)
                    f.r12 = offset
                    state = STATE_R12
                    offset -= 8
                if ( reg == '%rbx'):
                    #print('rbx found go to STATE_RBX , offset= ', offset)
                    f.rbx = offset
                    state = STATE_RBX
                    offset -= 8
            if ( state == STATE_R14 ):
                if ( reg == '%r13'):
                  #  print('r13 found go to STATE_R13 , offset= ', offset)
                    f.r13 = offset
                    state = STATE_R13
                    offset -= 8
                if ( reg == '%r12'):
                  #  print('r12 found go to STATE_R12 , offset= ', offset)
                    f.r12 = offset
                    state = STATE_R12
                    offset -= 8
                if ( reg == '%rbx'):
                  #  print('rbx found go to STATE_RBX , offset= ', offset)
                    f.rbx = offset
                    state = STATE_RBX
                    offset -= 8
            if ( state == STATE_R13 ):
                if ( reg == '%r12'):
                  #  print('r12 found go to STATE_R12 , offset= ', offset)
                    f.r12 = offset
                    state = STATE_R12
                    offset -= 8
                if ( reg == '%rbx'):
                  #  print('rbx found go to STATE_RBX , offset= ', offset)
                    f.rbx = offset
                    state = STATE_RBX
                    offset -= 8
            if ( state == STATE_R12 ):
                if ( reg == '%rbx'):
                  #  print('rbx found go to STATE_RBX , offset= ', offset)
                    f.rbx = offset
                    state = STATE_RBX
                    offset -= 8


        ln += 1
    if( state == STATE_RESET):
      #  print('NO STACK_FRAME')
        f.rbx = 100
        f.r15 = 100
        f.r14 = 100
        f.r13 = 100
        f.r12 = 100
        

#print(len(functions) , file=sys.stderr)
array_size = len(functions)

for function in functions:
        if (function.has_cold):
            cold_array_size +=1 
out_file.write("#ifndef __HECATON_DATA_FLAG\n")
out_file.write("#define __HECATON_DATA_FLAG\n")
out_file.write("#define HECATON_ARRAY_SIZE\t"+ str(array_size) + "\n")
out_file.write("#define HECATON_COLD_ARRAY_SIZE\t"+ str(cold_array_size) + "\n")


defstr = "uint64_t hecaton_fnc_begin_addrs[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + "0x" + function.base_addr + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr = "uint64_t hecaton_fnc_end_addrs[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + "0x" + function.end_addr + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr = "uint64_t hecaton_cold_begin_addrs[HECATON_COLD_ARRAY_SIZE] = {"
defstr_index = "uint64_t hecaton_cold_begin_index[HECATON_COLD_ARRAY_SIZE] = {"
index = 0
cold_index = 0
for function in functions:
        if (function.has_cold):
            defstr = defstr + "0x" + function.base_addr_cold + ", "
            defstr_index = defstr_index  + str(index) + ", "
            if cold_index%5==0:
                defstr = defstr + '\n'
                defstr_index = defstr_index + '\n'
            cold_index += 1    
      #  if index%5==0:
      #          defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr_index = defstr_index.rstrip('\n')
defstr_index = defstr_index.rstrip(' ')
defstr_index = defstr_index.rstrip(',')
defstr_index = defstr_index + "};\n"
out_file.write(defstr_index)

defstr = "uint64_t hecaton_cold_end_addrs[HECATON_COLD_ARRAY_SIZE] = {"
defstr_index = "uint64_t hecaton_cold_end_index[HECATON_COLD_ARRAY_SIZE] = {"
index = 0
cold_index = 0
for function in functions:
        if (function.has_cold):
            defstr = defstr + "0x" + function.end_addr_cold + ", "
            defstr_index = defstr_index  + str(index) + ", "
            if cold_index%5==0:
                defstr = defstr + '\n'
                defstr_index = defstr_index + '\n'
            cold_index += 1    
      #  if index%5==0:
      #          defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr_index = defstr_index.rstrip('\n')
defstr_index = defstr_index.rstrip(' ')
defstr_index = defstr_index.rstrip(',')
defstr_index = defstr_index + "};\n"
#out_file.write(defstr_index)


defstr = "int8_t hecaton_r15[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + str(function.r15)  + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr = "int8_t hecaton_r14[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + str(function.r14)  + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr = "int8_t hecaton_r13[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + str(function.r13)  + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)

defstr = "int8_t hecaton_r12[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + str(function.r12)  + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)


defstr = "int8_t hecaton_rbx[HECATON_ARRAY_SIZE] = {"
index = 0
for function in functions:
        defstr = defstr + str(function.rbx)  + ", "
        if index%5==0:
                defstr = defstr + '\n'
        index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)


defstr = "int8_t hecaton_supported[HECATON_ARRAY_SIZE] = {"
index = 0
for f in functions:
#    print(f.name, '\t', f.base_line, '\t', f.base_addr, '\t' , f.end_line, '\t' , f.end_addr , '\t\t', f.has_cold)
    if(f.name + '\n' in supp_lines):
        #print(f.name,file=sys.stderr)
        defstr = defstr + str(1)  + ", "
    else:
        defstr = defstr + str(0)  + ", "
    if index%5==0:
            defstr = defstr + '\n'
    index = index + 1
defstr = defstr.rstrip('\n')
defstr = defstr.rstrip(' ')
defstr = defstr.rstrip(',')
defstr = defstr + "};\n"
out_file.write(defstr)


out_file.write("#endif\n")

