# Fresh Study Evidence And Paper Revision

Status: COMPLETE

## Goal

Replace the manuscript's performance argument with the user-collected spec 014
sessions and address the September 30 independent reviews. Strengthen the full
paper before choosing a venue. The two pilots are descriptive same-host
evidence, not retroactively frozen confirmation experiments.

## Requirements And Examples

-   Validate both handbacks, all expected processes, source identities,
    numerical audits, execution records and sleep protection before analysis.
-   Preserve the raw sessions and historical evidence. Replace old results in
    the active manuscript; history need not remain in its reading path.
-   Derive compact process-level data, session summaries, tables, figures and
    prose numbers reproducibly. Keep session/process replication distinct from
    dependent callbacks, hops, analyzers and channels. Retain slow observations.
-   Compare native layouts, matched scheduling controls, horizons, complete
    modules and the actual Rack engine. Separate continuous compute cost,
    paced release misses, observed maxima, logical publication age and consumed
    age bounds. For example, a missed synthetic release is not an audio underrun.
-   Report residual OS activity and numerical decay floors explicitly. A
    COMPLETE session is evidence of protocol completion, not an idle OS.
-   Rewrite abstract, introduction, method, results, discussion and conclusion
    around supported findings, and map each substantive review concern to
    evidence, a scope decision or remaining work.
-   Build and visually review the PDF, run artifact checks, and compile the
    portable source export. Keep limitations and exact commands here.

## Non-Goals

No new performance collection, production DSP changes, winner-driven filtering,
confirmation promotion, public upload, submission, or venue-specific format
conversion. Raw historical files remain available for provenance.

## Acceptance And Validation

- [x] Both original session directories and archives validate without mutation.
- [x] A reproducible importer and checked publication generator cover all 776
      processes; every selected result has a traceable source.
- [x] The paper uses only the new study for current performance claims and
      distinguishes exploratory comparisons from verified algorithmic bounds.
- [x] Reviewer concerns and outstanding experiments are recorded concretely.
- [x] `make -C docs/whitepaper check` passes.
- [x] `make -C docs/whitepaper` and `make -C docs/whitepaper arxiv` pass;
      the extracted source compiles and rendered pages are reviewed.

## Evidence

September 30, 2026: found COMPLETE pilot-01 and pilot-02 under
`/Users/christiankauten/Fourier-benchmarks/spec014`. Both use prepared manifest
`a53795c22f18c1aa20fb1cf54e08a3f8a14a3fef896aafdd90ce6f7bf29d34a4`.
Host snapshots report AC and Low Power Mode off, with residual macOS
service activity. Full integrity and statistical checks passed.

### Completed Revision

September 30, 2026: completed manuscript version 4, *Whole-Pipeline Scheduling
for Real-Time Spectral Analysis*. Replaced current performance sections and
generated assets with the fresh study. Preserved original sessions/archives and
old research artifacts outside the active performance argument. The full report
is 32 pages, including derivations and references; venue-specific compression,
template and anonymity remain outside this task.

The reproducible importer retains all 776 process identities and validates the
predeclared 194-cell plan, both session orders, source/artifact hashes, numerical
audits and execution policies. The publication generator emits all 194 cell
rows, 388 session summaries, 776 process rows, selected tables, two measured
vector plots, macros and provenance. A third schematic figure explains the
whole-pipeline schedule. Added the conditional weighted cost bound with its
first-credit boundary carry term and disclosed historical weight tuning.

Fixed a reader-only JSON round-trip defect: engine consumer-age bounds were
returned as tuples but stored as lists. Values and measured code are unchanged.
The original engine hop diagnostic spans endpoint to publication, which is
asymmetric for immediate batch. Publication analysis instead derives complete
endpoint-to-next-endpoint hops from raw coordinates for every implementation.
Partial edges, shared blocks and dependent analyzer observations are explicit.

The principal actual-host contrast is four aligned Fourier modules on one
thread: typical hop peaks 201 us (production), 890 us (vDSP batch), and 423 us
(native hybrid), against a 1,333 us block budget. Production costs more there.
Native hybrids also win on both recorded cost and peak at some controlled
shapes. Release misses have no consistent ranking. The empty four-thread engine
uses approximately three cores of process CPU; the paper distinguishes this
activity-regime effect from a causal barrier-cost claim. Residual OS activity,
FTZ-tail accuracy, one-host/date replication and absent device evidence remain
visible in methods, results and discussion.

See [the review response](https://github.com/Arhythmetic-Units/Fourier/blob/a7650346cfb81d9bb454d4d470a45b5dd5c39d77/docs/whitepaper/reviews/revision-4-response.md)
for addressed concerns and concrete remaining experiments. No performance
collection, production DSP change, public upload or submission was performed.

### Validation Actually Run

From the repository root:

```shell
python3 docs/whitepaper/tools/study_import.py \
  /Users/christiankauten/Fourier-benchmarks/spec014
python3 docs/whitepaper/tools/study_import.py \
  /Users/christiankauten/Fourier-benchmarks/spec014 \
  --output /tmp/fourier-study-014-rederived
python3 docs/whitepaper/tools/study_paper.py
python3 docs/whitepaper/tools/study_paper.py --check
python3 -m unittest discover -s docs/whitepaper/tools -p test_study_import.py
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests
make -C docs/whitepaper check
make -C docs/whitepaper arxiv
make -C docs/manual-fourier
make -C docs/manual-spectre
git diff --check
```

-   Both raw-data imports passed all 776 processes. The second import into
    `/tmp/fourier-study-014-rederived` is byte-identical for every compact
    evidence file and the receipt. Original archive checksums and directory
    hashes still match. Logs: `/tmp/fourier-study-import-final.log` and
    `/tmp/fourier-study-rederive.log`.
-   Six new analysis regression cases passed, including complete-hop batch
    coverage, edge/shared-block handling, changing sample-rate budgets,
    release/compute separation and weighted first-credit carry.
-   Benchmark suite: 126 tests, 125 passed, one optional plotting skip. This
    includes the actual-engine correctness/fake-clock fixtures and JSON
    round-trip regression. Log: `/tmp/fourier-paper-python-tests.log`.
-   Artifact check passed all current generated assets, 35 citations, links,
    historical hashes/tables and 32,768 quota cases. Publication checks require
    no raw-data access or new timing collection.
-   Paper and portable source archive built. Extracted into
    `docs/whitepaper/.build/portable-v4`, then ran
    `latexmk -pdf -pdflatex='pdflatex -no-shell-escape %O %S' fourier.tex`
    there. Its extracted PDF text is identical to the main build. Logs:
    `/tmp/fourier-paper-export-reviewed.log` and
    `/tmp/fourier-paper-portable-reviewed.log`.
-   Rendered-page overview and detailed checks covered the complete report,
    with detailed checks of all new figures/tables. Fixed a legend overlap and
    a split conclusion; final build has no unresolved references or overfull
    boxes. One harmless underfull box in a narrow table cell remains.
-   Both manuals built after updating only the paper-title citation. Reviewed
    their affected further-reading pages. No panel/control behavior changed,
    so no module screenshot refresh was required.
-   No production C++ or plugin binary changed. No new Rack plugin build or
    interactive Rack/audio-device test is claimed for this manuscript revision.
