all: main.dvi main.pdf

main.dvi: main.tex ref.bib
	platex -shell-escape main.tex
	pbibtex main.aux
	platex -shell-escape main.tex
	platex -shell-escape main.tex

main.pdf: main.dvi
	dvipdfmx -p letter main.dvi

clean:
	rm -f main.pdf *.dvi *.xbb *~
