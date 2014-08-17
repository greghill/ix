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

ALIAS = {}
REVALIAS = {}

cachefile = ".bibcloud/DBLP.xml"
aliasfile = "dblp-alias.txt"


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
                os.system("curl http://dblp.uni-trier.de/rec/bibtex/"+key+ ".xml >.bibcloud/tmp.xml")
                if os.path.getsize(".bibcloud/tmp.xml") >0:
                    #special case -- no existing XML tree
                    if num_children == 0:
                        tree = ET.parse(".bibcloud/tmp.xml")
                        root = tree.getroot()
                    else:
                        newtree = ET.parse(".bibcloud/tmp.xml")
                        root.insert(num_children,newtree.getroot()[0])

                    num_children = num_children + 1
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
    u"á" : "{\\'a}"
}
 

def html_to_bibtex(h):
    try:
        return str(h)
    except:
        x = ""
        for c in h:
            if HTML_TO_BIB.has_key(c):
                x = x + HTML_TO_BIB[c]
            else: 
                x = x + c
        print "HTML conversion ",h," --> ",x
        return x
    

##### main  #######                                                                                                                                                 
# process bib file from ARVG

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
    print "File "+bibname+".aux does not exist"
    exit

if os.path.isfile(aliasfile):
    lines = [line.strip() for line in open(aliasfile)]
    for l in lines:
        # remove comments
        pos = l.find("%")
        if pos >=0:
            l = l[:pos]

        x = l.split(" ")
        if len(x)>=2 and x[1].find("DBLP:")>=0:
            print "found alias ",x[0],x[1]
            ALIAS[x[0]] = x[1]
            REVALIAS[x[1]] = x[0] 

       
else:
    print "no ",aliasfile," file!"
    
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
            print "ARTICLE ",c
            F.write("  author = {"+  " and ".join(authorlist) + "},\n")
            for a in xml:
                if DBLP_article_fieldlist.has_key(a.tag):
                    if DBLP_article_fieldlist[a.tag] == 'id':
                        F.write("  "+a.tag+" = {"+a.text+"},\n")
                    elif DBLP_article_fieldlist[a.tag] == 'double':
                        F.write("  "+a.tag+" = {{"+a.text+"}},\n")
                    else:
                        print "BAD code",DBLP_article_fieldlist[a.tag]
                        sys.exit()
    
        elif xml.tag == "inproceedings":
            F.write("\n@inproceedings{"+cit+",\n")
            print "INPROCEEDINGS ",c,cit
            F.write("  author = {"+  " and ".join(authorlist) + "},\n")
            for a in xml:
                if DBLP_conf_fieldlist.has_key(a.tag):
                    if DBLP_conf_fieldlist[a.tag] == 'id':
                        F.write("  "+a.tag+" = {"+a.text+"},\n")
                    elif DBLP_conf_fieldlist[a.tag] == 'double':
                        F.write("  "+a.tag+" = {{"+a.text+"}},\n")
                    else:
                        print "BAD code",DBLP_conf_fieldlist[a.tag]
                        sys.exit()
            year = xml.find('year').text[2:]
            booktitle = xml.find('booktitle').text
            if booktitle.find(" ")>0: 
                if booktitle == "USENIX Annual Technical Conference, General Track":
                    booktitle = "USENIX"
                elif booktitle == "USENIX Annual Technical Conference":
                    booktitle = "USENIX"
                else:
                    print "Unknonw conference",booktitle
                    sys.exit(1)
            F.write("  booktitle = "+booktitle+year+",\n")
            
        if c != cit:
             F.write("  bibsource = {DBLP alias: "+c+"}\n")
        F.write("}\n")

        





