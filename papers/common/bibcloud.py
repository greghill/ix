#!/usr/bin/python
# -*- coding: latin-1 -*-

####
####  bibcloud.py
####
####  usage:  bibcloud.py <main> where <main> is the main latex file (without .tex extension)
####      - run this AFTER pdflatex and BEFORE bibtex
####      - in the $cwd, "bibalias.txt" can be used to alias pretty names to full DBLP citations
####          (each line: <pretty> <dblp>)
####
####  bibcloud does the following:
####     - it scans the <main>.aux file for references from DBLP
####     - maintains a cache of downloaded references in ./bibcloud/*
####     - automatically downloads missing references into the cache from http://dblp.uni-trier.de
####     - automatically generates dblp.bib in the $cwd.  In doing so, it fixes the classic issues associated with bibtex downloads, 
####            incl. double {{}} for title
####            genearting a per-year specific booktitle for conference proceeedings
####
####  things that you still have to do manually
####     - maintain the file abbrev.bib which contains names of conference proceedings (error: missing booktitle)
####     


import sys
import os
import xml.etree.ElementTree as ET
import subprocess
import time
import locale

    

DBLP_article = {}
DBLP_article_fieldlist = {'title':'double',
                      'journal':'id',
                      'volume':'id',
                      'number':'id',
                      'year':'id',
                      'pages':'id'}



DBLP_conf_fieldlist = {'title':'double',
                      'year':'id',
                      'pages':'id'}


# conferences where DBLP does not use an acronym
NOACKCONFERENCE = {
    "ACM Conference on Computer and Communications Security" : "ACM Conference on Computer and Communications Security",
    "USENIX Annual Technical Conference, General Track" :  "USENIX Annual Technical Conference",
    "USENIX Annual Technical Conference" : "USENIX",
    "Integrated Network Management" : "Integrated Network Management",
    "Virtual Machine Research and Technology Symposium": "Virtual Machine Research and Technology Symposium",
    "Workshop on I/O Virtualization" : "Workshop on I/O Virtualization",
    "Best of PLDI" : "Best of PLDI",
    "SIGMOD Conference" : "SIGMOD Conference",
    "IEEE Symposium on Security and Privacy" : "IEEE Symposium on Security and Privacy",
    "USENIX Summer" : "USENIX Summer",
    "USENIX Annual Technical Conference, FREENIX Track" : "USENIX Annual Technical Conference, FREENIX Track",
    "Internet Measurement Conference" : "IMC",
    "Internet Measurement Workshop" : "IMC"
}


ALIAS = {}
REVALIAS = {}
TITLESUB = {}

cachefile = ".bibcloud/DBLP.xml"
aliasfile = "dblp-alias.txt"
titlefixfile  = "dblp-title.txt"


##### extraction from bibtex .aux file
def find_citation(l):
    x = l.find("\\citation{")
    if (x>=0):
        y = l.find("{")
        z = l.find("}",y)
        return l[y+1:z]
    else:
        return ""


###### update dblp cache ########

def update_dblp():
    # Processing DBLP files
    print "processing DBLP XML files"
    num_children = 0
    try:
        tree = ET.parse(cachefile)
        root = tree.getroot()
        for child in root:
            num_children = num_children+1
            if child.tag == "article" or child.tag=="inproceedings":
                DBLP_article["DBLP:"+child.attrib['key']] = child
    except:
        print "No cache file found ... loading all from DBLP\n"

 ## finding missing DBLP files
    nr_misses = 0
    for cit in citations:
        if ALIAS.has_key(cit):
            c = ALIAS[cit]
        else:
            c = cit

        if c.find("DBLP")>=0:
            if not(DBLP_article.has_key(c)):
                key = c[5:]
                print "DBLP article miss",c,"|",key
                nr_misses = nr_misses + 1
                time.sleep(3)
                print "CURL ... curl http://dblp.uni-trier.de/rec/xml/"+key+".xml"
                os.system("curl http://dblp.uni-trier.de/rec/xml/"+key+ ".xml >.bibcloud/tmp.xml")
                if os.path.getsize(".bibcloud/tmp.xml") >0:
                    #special case -- no existing XML tree
                    try:
                        if num_children == 0:
                            tree = ET.parse(".bibcloud/tmp.xml")
                            root = tree.getroot()
                        else:
                            newtree = ET.parse(".bibcloud/tmp.xml")
                            root.insert(num_children,newtree.getroot()[0])
                        num_children = num_children + 1
                        print "Updating cache ..."
                        tree.write(".bibcloud/DBLP.xml")
                    except:
                        os.system("mv .bibcloud/tmp.xml .bibcloud/error.xml")
                        print "ERROR in XML parsing ... see file ./bibcloud/error.xml"
             
                else: 
                    print "FETCH of ",key," failed..."

    if nr_misses>0:
        print "Updating cache ..."
        tree.write(".bibcloud/DBLP.xml")


########## html_to_bibtex ######
### brutal escaping

HTML_TO_BIB = {
    u'é' : "{\\'e}",
    u'ö' : "{\\\"o}",
    u'ä' : "{\\\"a}",
    u'É' : "{\\'E}",
    u'ü' : "{\\\"u}",
    u"é" : "{\\'e}",
    u"è" : "{\\`e}",
    u"á" : "{\\'a}",
    u"ç" : "\\c{c}",
    u"Ö" : "{\\\"O}",
    u"í" : "\\'{\\i}",
    u"ñ" : "\\~{n}"
}
 

def html_to_bibtex(h):
    try:
        return str(h)
    except:
        x = ""
        for c in h:
            c2 = c.encode('utf-8')
            if HTML_TO_BIB.has_key(c):
                x = x + HTML_TO_BIB[c]
            else: 
                x = x + c
        print "HTML conversion ",h.encode('utf-8')," --> ",x.encode('utf-8')
        return x.encode('utf-8')
    

def escape_percent(s):
    x = s.find("%")
    if x>=0:
        s2 = s[:x] + "\%" + escape_percent(s[x+1:])
        print "ESCAPING%: ",s,s2
        return s2
    else:
        return s


##### main  #######                                                                                                                                                 
# process bib file from ARVG



print "This is bibcloud ... Use at your own risk"
print "Usage bibcloud <latex main>"
print "Input: latex .aux file; ./dblp-alias.txt; ./dblp-title.txt"
print "Output: dblp.bib"
print "Files are cached in ./bibcloud (do not put under revision control)"

#for x in os.environ:
    #print "ENVIRON:",x, os.environ[x]

#print locale.localeconv()

if not os.path.exists(".bibcloud"):
    os.mkdir(".bibcloud")
    

bibname = sys.argv[1] + ".aux"
if (os.path.isfile(bibname)):
    print "Parsing ",bibname
    lines = [line.strip() for line in open(bibname)]
    lines =  [find_citation(line) for line in lines]
    lines  = [c.strip(" ") for c in lines if c != ""]
    citations = []
    for l in lines:
        if l not in citations:
            citations.append(l)
    citations.sort()

else:
    print "FATAL -- File "+bibname+" does not exist"
    sys.exit(1)

if os.path.isfile(aliasfile):
    lines = [line.strip() for line in open(aliasfile)]
    for l in lines:
        # remove comments
        pos = l.find("%")
        if pos >=0:
            l = l[:pos]

        x = l.split()
        if len(x)>=2 and x[1].find("DBLP:")>=0:
            #print "found alias ",x[0],x[1]
            ALIAS[x[0]] = x[1]
            REVALIAS[x[1]] = x[0]
        elif len(x)>0:
            print "Alias parsing - bad line : ",x

       
else:
    print "no ",aliasfile," file!"
    

if os.path.isfile(titlefixfile):
    lines = [line.strip() for line in open(titlefixfile)]
    for l in lines:
        # remove comments
        pos = l.find("%")
        if pos >=0:
            l = l[:pos]

        x = l.split("|")
        if len(x)==2:
            TITLESUB[x[0]] = x[1]
            #print "TITLE substitiution",x[0],x[1]



update_dblp()

# reload DBLP file
tree = ET.parse(cachefile)
root = tree.getroot()
num_children = 0
for child in root:
    num_children = num_children+1
    if child.tag == "article" or child.tag=="inproceedings":
        DBLP_article["DBLP:"+child.attrib['key']] = child
        


F = open("dblp.bib","w")
F.write("%%% This file is automatically genreated by bibcloud.py\n")
F.write("%%% DO NOT EDIT\n\n\n")

# generate dblp.bib file from XML
for cit in citations:
    if ALIAS.has_key(cit):
        c = ALIAS[cit]
    else:
        c = cit

    if REVALIAS.has_key(cit) and REVALIAS[cit] in citations:
        print "FATAL: Citation cannot be used both aliased and non-aliased: ",REVALIAS[cit],cit
        sys.exit(1)

    if DBLP_article.has_key(c):
        xml = DBLP_article[c] 
        authorlist = [a.text for a in xml if a.tag=="author"]
        authorlist = [html_to_bibtex(a) for a in authorlist]
        if xml.tag == "article":
            F.write("\n@article{"+cit+",\n")
            #print "ARTICLE ",c
            F.write("  author = {"+  " and ".join(authorlist) + "},\n")
            for a in xml:
                if DBLP_article_fieldlist.has_key(a.tag):
                    if DBLP_article_fieldlist[a.tag] == 'id':
                        F.write("  "+a.tag+" = {"+a.text+"},\n")
                    elif DBLP_article_fieldlist[a.tag] == 'double':
                        if a.tag == "title" and TITLESUB.has_key(a.text):
                            F.write("  "+a.tag+" = {{"+escape_percent(TITLESUB[a.text])+"}},\n")
                        else:
                            F.write("  "+a.tag+" = {{"+escape_percent(a.text)+"}},\n")
                    else:
                        print "BAD code",DBLP_article_fieldlist[a.tag]
                        sys.exit()
    
        elif xml.tag == "inproceedings":
            F.write("\n@inproceedings{"+cit+",\n")
            #print "INPROCEEDINGS ",c,cit
            F.write("  author = {"+  " and ".join(authorlist) + "},\n")
            for a in xml:
                if DBLP_conf_fieldlist.has_key(a.tag):
                    if DBLP_conf_fieldlist[a.tag] == 'id':
                        F.write("  "+a.tag+" = {"+a.text+"},\n")
                    elif DBLP_conf_fieldlist[a.tag] == 'double':
                        if a.tag == "title" and TITLESUB.has_key(a.text):
                            F.write("  "+a.tag+" = {{"+escape_percent(TITLESUB[a.text])+"}},\n")
                        else:
                            F.write("  "+a.tag+" = {{"+escape_percent(a.text)+"}},\n")
                    else:
                        print "BAD code",DBLP_conf_fieldlist[a.tag]
                        sys.exit()
            year = xml.find('year').text[2:]
            booktitle = xml.find('booktitle').text
            if booktitle.find(" ")>0:
                if NOACKCONFERENCE.has_key(booktitle):
                    booktitle = NOACKCONFERENCE[booktitle]
                    if booktitle.find(" ")>0:
                        F.write("  booktitle = \""+booktitle+"\",\n")
                    else:
                        F.write("  booktitle = "+booktitle+year+",\n")
                else:
                    print "FATAL -- Unknown conference",booktitle
                    sys.exit(1)
            else:
                F.write("  booktitle = "+booktitle+year+",\n")
            
        if c != cit:
             F.write("  bibsource = {DBLP alias: "+c+"}\n")
        F.write("}\n")

print "bibcloud SUCCESS; dblp.bib updated"
        





