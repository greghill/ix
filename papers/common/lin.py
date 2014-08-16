#!/usr/bin/python

import sys;
import os;


def find_command(l,cmd):
    x = l.find(cmd+"{")
    if (x>=0):
        y = l.find("{",x)
        z = l.find("}",x)
        return [l[:x],l[y+1:z],l[z+1:]]
    else:
        return [l,"",""]




def remove_comment(l):
    b = 0
    while True:
        x = l.find("\\%",b)
        if (x<0):
            break
        else:
            print "XXX skip percentage",x,l
            b = x+2
        
    x = l.find("%",b)
    if x>0:
        print "XXX stripped comment",x,l,"|",l[:x]
        return  l[:x]
    else:
        return l
            

def expand_input(l):
        x = find_command(l,"\\input")
        if (x[1]!=""):
            print "XXX expand_input",x[1]
            return [""] + parse_file(x[1]+".tex") + [""]
        else: 
            return l


def flatten(L):
    x = []
    for l in L:
        if isinstance(l,list):
            x = x + flatten(l)
        else:
             x = x + [l]
        
    return x

def find_any_command(s):
    x = s.find("\\")
    if (x>=0):
        y = l.find("{",x)
        z = l.find("}",x)

        #only one argument for now.... fix later
        arg = [l[y+1,z]]
        return [l[:x],l[x+1,y-1],arg,l[z+1:]]
    else:
        return [l,"",[],""]

    
# merge_paragraph: consecutive lines are merged unless there is a blank in the middle

# input: List[String]
        
def merge_paragraph(L):
    x = [""]
    accu = ""
    for l in L:
        if l == "":
            x.append(accu)
            accu = ""
        else:
            accu = accu  + " " + l
            
    x.append(accu)
    return x


###### parse_file #####
def parse_file(f):
    lines = [line.strip() for line in open(f)]

    # remove lines with a leading % ... ->
    lines = [line for line in lines if line.find('%')!=0]

    # remove comments at right of line
    lines = [remove_comment(line) for line in lines]
    # expand all \input{} commands (recursively)
    lines = [ expand_input(line) for line in lines]
    # flatten to a list of strings
    lines = flatten(lines)
    return lines


##### apply_commands #####

def apply_command(l,cmd,sub):
    x = l
    while True:
        y = find_command(x,cmd)
        if y[1]!="":
            x = y[0] + sub + y[2];
        else:
            return x

        
def apply_commands(l):
    x = apply_command(l,"~\\cite"," [REF]")
    x = apply_command(x,"\\S\\ref"," SECTION")
    x = apply_command(x,"~\\ref"," FIG ")
    x = apply_command(x,"\\label","")

    x = apply_command(x,"\\dm","COMMENT. ")
    x = apply_command(x,"\\ana","COMMENT. ")
    x = apply_command(x,"\\george","COMMENT. ")


    return x



    
 
###### main  #######

fname = sys.argv[1]
if (os.path.isfile(fname)):
    print fname
    lines = parse_file(fname)


    lines = merge_paragraph(lines)

    lines = [apply_commands(l) for l in lines]

    for l in lines:
        print l


