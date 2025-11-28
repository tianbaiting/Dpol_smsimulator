# Reports Directory

This directory contains the technical report and presentation materials for the dpol_breakup experiment optimization project (November 26, 2025).

## 文件说明 / File Descriptions

### Main Documents

1. **report_20251126_en.md** (Markdown source)
   - Technical progress report in English
   - Includes: objectives, system architecture, benchmarks, QMD processing, Geant4 simulation, PDC reconstruction
   
2. **report_20251126_en.pdf** (PDF version, 1.2MB)
   - Generated from Markdown using Pandoc
   - Formatted with table of contents and numbered sections
   - Ready for distribution to Mizuki and Aki

3. **presentation_script_20251126.md** (Bilingual speaking notes, 19KB)
   - English/Chinese parallel text for oral presentation
   - Conversational tone with time allocation guidance
   - 13 main sections + backup content

4. **presentation_20251126.tex** (LaTeX Beamer source)
   - Professional academic presentation slides
   - Madrid theme, 16:9 aspect ratio
   - 20+ slides with graphics, tables, equations

5. **presentation_20251126.pdf** (Compiled Beamer presentation)
   - Final presentation PDF
   - Ready for delivery

### Assets

- **assets_20251126_en/** - Image directory (11 PNG files)
  - QMD data flow diagrams
  - Geant4 visualization screenshots
  - Detection efficiency plots
  - PDC reconstruction results

### Compilation Scripts

- **compile_beamer.sh** - Automated script to compile LaTeX Beamer presentation
  - Runs pdflatex twice for proper TOC generation
  - Cleans up auxiliary files

## 如何重新生成PDF / How to Regenerate PDFs

### Report PDF (from Markdown)
```bash
pandoc report_20251126_en.md -o report_20251126_en.pdf \
  --pdf-engine=xelatex \
  -V geometry:margin=1in \
  -V fontsize=11pt \
  --toc --toc-depth=3 \
  --number-sections
```

**Requirements:** Pandoc and XeLaTeX

### Presentation PDF (from LaTeX Beamer)
```bash
# Option 1: Use the compilation script (recommended)
./compile_beamer.sh

# Option 2: Compile manually
pdflatex presentation_20251126.tex
pdflatex presentation_20251126.tex  # Run twice for TOC and references
```

**Requirements:** LaTeX distribution (texlive-latex-base, texlive-latex-extra, texlive-fonts-recommended)

## Notes

- All image paths are relative: `assets_20251126_en/image*.png`
- Report contains 11 figures with detailed captions
- Presentation includes TikZ diagrams, code listings, and mathematical equations
- All materials prepared for presentation to Mizuki and Aki

## Version History

- **2025-11-26**: Initial creation
  - Technical report finalized
  - Bilingual presentation script completed
  - LaTeX Beamer slides created
  - All PDFs generated successfully
