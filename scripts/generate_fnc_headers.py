import sys 
if ( len(sys.argv) !=2 ):
    print("usage: generate_fnc_headers.py bugdir ")

f = open ( sys.argv[1] + 'functions.txt' )
fp_out = open( sys.argv[1] + 'fncs_header.h','w')
lines = f.readlines()
size_str  = "#define DATA_SIZE " + str(len(lines)) +'\n'
fn_str = "std::string white_list[DATA_SIZE] = {\n"
for line in lines:
    l = '"' + line.rstrip() + '",\n'
    fn_str += l
fn_str = fn_str.rstrip('\n')
fn_str = fn_str.rstrip(',')
fn_str = fn_str + '};\n'
fp_out.write(size_str);
fp_out.write(fn_str);

