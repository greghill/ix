PAPER=osdi

all: pdf
pdf: $(PAPER).pdf

clean:
	$(RM) *.aux *.log *.bbl *.blg *~

spell:
	../common/linearize.pl word $(PAPER).tex; open -a /Applications/Microsoft\ Office\ 2011/Microsoft\ Word.app/ latex-all.tex

%.pdf: $(wildcard *.tex) $(wildcard *.bib)
	pdflatex $(PAPER)
	bibtex $(PAPER)
	pdflatex $(PAPER)
	bibtex $(PAPER)
	pdflatex $(PAPER)
	$(RM) $*.dvi *.aux *.log *.bbl *.blg *.toc *.tmp.ps
