correct = open("nestest.log")
mine = open("my.log")

def clean(line): 
    return line[:9] + line[48:73] + "\n"
    

i = 1
while (1):
    c = clean(correct.readline())
    m = mine.readline()
    if not c == m:
        print("difference at line %d"%i)
        print("correct: "+c)
        print("   mine: "+m)
        break
    i += 1

print("Success")
