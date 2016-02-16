import os,sys
import random

basedir = "/Users/twongjirad/working/larbys/data"

classes_info = {0:{"folder":"single_eminus/rootfiles",  "particle":"eminus"},
                1:{"folder":"single_muminus/rootfiles", "particle":"muminus"},
                2:{"folder":"single_proton/rootfiles",  "particle":"proton"},
                3:{"folder":"single_pizero_bnblike/rootfiles", "particle":"pizero"}}

outfile = sys.argv[1]

lines_train = []
lines_validate = []

for iclass in classes_info.keys():
    files = os.listdir( basedir+"/"+classes_info[iclass]["folder"] )
    classfiles = []
    for f in files:
        if ".root" in f:
            l = basedir+"/"+classes_info[iclass]["folder"] + "/" + f.strip() + " %d" % (iclass)
            classfiles.append(l)
    # split between training and validation
    classfiles.sort()
    train = classfiles[:len(classfiles)/2]
    valid = classfiles[len(classfiles)/2:]
    for v in valid:
        lines_validate.append(v)
    for t in train:
        lines_train.append(t)
    print "Class ",classes_info[iclass]["particle"],": ",len(classfiles)," = ",len(train)," (train) + ",len(valid)," (valid)"
            
                
random.shuffle( lines_train )
random.shuffle( lines_validate )

out_valid = file( outfile+"_validate.txt",'w')
for f in lines_validate:
    print >> out_valid,f
out_valid.close()

out_train = file( outfile+"_train.txt",'w')
for f in lines_train:
    print >> out_train,f
out_train.close()
