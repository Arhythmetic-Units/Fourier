# Scheduling FFT-Based Spectral Analysis In The Audio Processing Loop

A seven-page, two-column conference-style preprint by Christian Kauten
(Arhythmetic Units), release v2.3.0 (manuscript version 5), dated
October 1, 2026. It explains how to distribute FFT-based analysis across sample
calls and evaluates the tradeoff between processing bursts, total work, and
result age.
The seven pages include the abstract and references. The paper has not been
peer reviewed by a venue, deposited on arXiv, or assigned a DOI.

The approach is called **CoopFFT** (Cooperative Fast Fourier Transform): a
resumable FFT and surrounding analysis pipeline that advance under per-call
work quotas. In comparisons, **CoopFFT scheduled** spreads that work across
sample calls; **CoopFFT batch** runs the same implementation at the frame
endpoint. The name applies independently of VCV Rack. The retained benchmark
identifiers, including `core-matched-{batch,distributed}-{float,double}`, keep
their original names for reproducibility.

## Read And Build

[fourier.tex](fourier.tex) is the complete, canonical manuscript. Like the
[RackNES paper](https://github.com/Kautenja/RackNES/tree/master/whitepaper), it
contains its prose, typography, figures, tables, and bibliography in one file.
Open it directly in the Codex LaTeX editor for editing and PDF preview.

For a command-line build, install `latexmk`, `pdflatex`, and the packages in the
preamble, then run from the repository root:

```shell
make -C docs/whitepaper
```

The result is `docs/whitepaper/.build/paper.pdf`. Typesetting uses only
`fourier.tex`; it does not require Python, derived assets, or benchmark tools.
Successful builds remove auxiliary files. Failed builds retain diagnostics.

To share the standalone source:

```shell
make -C docs/whitepaper source
```

Extract `.build/fourier-arxiv-source.tar.gz` into an empty directory and run
`latexmk -pdf -pdflatex='pdflatex -no-shell-escape %O %S' fourier.tex`.
The `arxiv` target additionally builds and checks the paper before completing.
Neither target publishes or submits it. `clean` removes the PDF, source archive,
and derived study assets while preserving measured evidence.

## Evidence And Maintenance

Maintain the paper in `fourier.tex`. The [source notes](sources.md) and
[study evidence guide](data/study-014/README.md) document its references,
measurements, identities, aggregation, and limitations. The
[benchmark workflow](benchmarks/README.md) and [validation tools](tools/README.md)
remain separate from typesetting.

From the repository root, verify references, links, metadata, scheduling
examples, and the embedded study results with:

```shell
make -C docs/whitepaper check
```

The check independently derives results in memory and compares the four marked
study blocks in `fourier.tex` exactly. It writes no generated assets and never
edits the manuscript or collects timings. If measured results change, regenerate
with `make -C docs/whitepaper generate` and update those marked blocks from
the corresponding generated files in ignored `.build/paper-study/`. All
cell/process/session CSVs remain available beside the generated tables.

The [deprecated archive](../latex/deprecated/whitepaper/README.md) preserves the
extended report, appendices, older experiments, and reproduction tools with
its own build and checks. They are excluded from the conference source archive.
The [housekeeping specification](../../specs/archive/017-whitepaper-housekeeping.md)
records content and rendering equivalence.

## Cite

[CITATION.bib](CITATION.bib), [CITATION.cff](../../CITATION.cff), and the
[project README](../../README.md#citation) identify
*Scheduling FFT-Based Spectral Analysis in the Audio Processing Loop*,
release v2.3.0 (manuscript version 5). After a public deposit, update them with
its actual identifier and URL. Source and artwork terms remain in
[LICENSING.md](../../LICENSING.md).
