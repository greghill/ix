PAPER=osdi

all: pdf
pdf: $(PAPER).pdf

clean:
	rm -f $(PAPER).pdf prepress.pdf *.aux *.fdb_latexmk *.log *.bbl *.blg \
		*~ *.dvi *.vrb *.nav *.snm texput.* *.synctex.gz
	latexmk -C $(PAPER).tex

spell:
	../common/linearize.pl word $(PAPER).tex; open -a /Applications/Microsoft\ Office\ 2011/Microsoft\ Word.app/ latex-all.tex

$(PAPER).pdf:
	latexmk -pdf -latexoption=-halt-on-error \
		-latexoption=-file-line-error \
		-latexoption=-synctex=1 $(PAPER).tex \
		&& touch $(PAPER).dvi || ! rm -f $@
	#$(RM) $*.dvi *.aux *.log *.bbl *.blg *.toc *.tmp.ps

prepress.pdf: $(PAPER).pdf
	ps2pdf -dPDFSETTINGS=/prepress $(PAPER).pdf prepress.pdf

.PHONY: $(PAPER).pdf pdf all clean prepress.pdf
