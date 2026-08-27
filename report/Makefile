# Build the report PDF. Requires a TeX distribution (MacTeX / TeX Live).
# `make`        -> accumulator_pricing.pdf   (runs pdflatex twice for TOC + refs)
# `make clean`  -> remove aux files

TEX = accumulator_pricing

$(TEX).pdf: $(TEX).tex
	pdflatex -interaction=nonstopmode -halt-on-error $(TEX).tex
	pdflatex -interaction=nonstopmode -halt-on-error $(TEX).tex

.PHONY: clean
clean:
	rm -f $(TEX).aux $(TEX).log $(TEX).out $(TEX).toc
