#!/bin/bash

doxygen ./doxy.cfg
sed -i -e 's,begin{document},usepackage{CJKutf8}\n\\begin{document}\n\\begin{CJK}{UTF8}{gbsn},' docs/latex/refman.tex
sed -i -e 's,end{document},end{CJK}\n\\end{document},' docs/latex/refman.tex
cd docs/latex/; make
