PAPER=osdi

all: pdf
dvi: $(PAPER).dvi
ps:  $(PAPER).ps
pdf: $(PAPER).pdf

clean:
	$(RM) *.aux *.log *.bbl *.blg *~

%.pdf: %.dvi $(wildcard *.bib)
	dvips -Ppdf -G0 -t letter -o $*.tmp.ps $<
	ps2pdf14 -dCompatibilityLevel=1.3 -dPDFSETTINGS=/prepress -dEmbedAllFonts=true $*.tmp.ps $*.pdf
	$(RM) $*.dvi *.aux *.log *.bbl *.blg *.toc *.tmp.ps

%.dvi: $(wildcard *.tex) $(wildcard *.bib)
	latex $*
	bibtex $*
	latex $*
	latex $*
	$(RM) *.aux *.log *.bbl *.blg *.toc
