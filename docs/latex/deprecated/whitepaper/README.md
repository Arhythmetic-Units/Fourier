# Deprecated Whitepaper Material

This archive preserves the extended version 4 report, its supporting sources,
and earlier experiments. The active seven-page conference manuscript lives in
[docs/whitepaper](../../../whitepaper/README.md). Nothing here enters its PDF or
portable source export.

## Build And Check

From the repository root, with the same Python 3 and LaTeX prerequisites as the
active paper:

```shell
make -C docs/latex/deprecated/whitepaper
make -C docs/latex/deprecated/whitepaper check
```

The build writes `.build/report-v4.pdf` here. It regenerates the extended
study tables and plots from the active paper's unchanged
[study 014 evidence](../../../whitepaper/data/study-014/README.md), using shared
aggregation code. The 31-page report retains its original text, labels,
reading order, and typography. Some historical reproduction paths printed in
the report describe the original checkout layout; use this guide for the
current locations.

`check` independently validates the original FFT and pipeline evidence and
rederives the spec 012 comparison summaries. It does not collect timings or
require the removed bulk plot exports. `clean` removes this report's generated
output, preserving retained measurements and source archives.

## Source Map

| Location | Contents |
| --- | --- |
| [fourier.tex](fourier.tex), [preamble.tex](preamble.tex), [bibliography.tex](bibliography.tex) | Version 4 reading order, typography, and references |
| [sections/](sections/) | Extended report and unused earlier main-text sources |
| [appendices/](appendices/README.md) | Derivations, scheduling proofs, implementation details, and historical experiments |
| [figures/](figures/), [tables/](tables/) | Historical editorial figures and tables |
| [data/](data/README.md) | Original FFT evidence, pipeline prototype, research campaigns, and spec 012 handoff |
| [benchmarks/history/](benchmarks/history/README.md) | Older configurations and prototype reproduction scripts |
| [tools/](tools/README.md) | Historical checks and extended publication generation |
| [sources.md](sources.md) | Broader literature and provenance notes |
| `.build/` | Ignored derived assets and compiled report |

## Evidence Preservation

Measured data, manifests, receipts, compressed metadata, and source archives
retain their original bytes and hashes. The spec 012 handoff's `README.md` and
`bundles/README.md` are themselves hashed evidence and remain unchanged. Their
relative paths and recorded commands describe the old `docs/whitepaper` layout.
Older `data/`, `benchmarks/history/`, and historical manuscript sources now live
under this archive; maintained collection tools remain in
[the active benchmark directory](../../../whitepaper/benchmarks/README.md).

The previously removed bulk generated exports and review files are not restored
by this refactor. Their historical snapshot remains in
[Git at the completed rewrite](https://github.com/Arhythmetic-Units/Fourier/tree/a7650346cfb81d9bb454d4d470a45b5dd5c39d77/docs/whitepaper).
The spec 012 manifest still records those exports' original hashes; the current
archive check validates retained evidence separately. Local ignored campaign
bundles move with their evidence directory and remain local, not a public data
deposit. Do not rewrite recorded identities to match a newer tool or layout.
