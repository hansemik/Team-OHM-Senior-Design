import csv

input_file = "temp.csv"
output_file = "out.csv"

with open(input_file, 'r') as f:
    with open(output_file, 'w') as t:
        for lines in f:
            s = lines.split(',')
            for col in s:
                if (col == '\n'):
                    t.write("\n")
                    continue
                c = int(col,16)
                c = c/26214
                t.write(str(c))
                t.write(',')
            #print(lines)
            #new_line = lines.replace(".",",")
            #t.write(new_line)

#for row in csv.reader(output_file):
#    for col in row:
#        print(int(col,16))
