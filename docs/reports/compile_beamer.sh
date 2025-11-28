#!/bin/bash
# Script to compile the Beamer presentation
# Usage: ./compile_beamer.sh

echo "Compiling Beamer presentation..."

# Compile twice for references and table of contents
pdflatex presentation_20251126.tex
pdflatex presentation_20251126.tex

# Clean up auxiliary files
rm -f *.aux *.log *.nav *.out *.snm *.toc *.vrb

echo "Done! PDF generated: presentation_20251126.pdf"
