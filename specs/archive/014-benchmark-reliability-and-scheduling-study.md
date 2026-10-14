# Benchmark Reliability And Scheduling Preparation

Prepare validated benchmark implementations and simple offline launch commands
for the user to collect reliable measurements on a quiet laptop. The intended
evidence will support the full paper's discussion of scheduling, cost, and
execution granularity. Collecting and interpreting it are separate follow-ups.

Status: COMPLETE

Created: September 30, 2026

## Goal And Execution Boundary

Extend the existing benchmark workflow, implement focused experimental
variants, verify their correctness, and deliver a prepared local package with
the exact commands the user needs to launch measurements. Finish each phase's
preparation checks before dependent work; retain decisions and validation
evidence in this spec. Completion ends at the runnable handoff, without
requiring benchmark numbers, a performance winner, or manuscript changes.

The current implementation request covers all remaining phases, completed and
committed sequentially with evidence below. No measurements have been collected
by the agent. The user prefers strengthening the full paper before
choosing a venue and retaining the M1 Pro as the primary test platform. A second
architecture is optional follow-up; cross-architecture claims require actual
measurements on that architecture.

Implementation of this spec authorizes builds, dependency preparation, untimed
correctness/allocation/concurrency checks, and tests using synthetic timing
records, fake clocks, or stub measurement processes. It does not authorize the
agent to launch performance measurements, including development benchmarks,
timed smoke tests, pilots, calibration campaigns, or confirmation runs. Adapt
tests that currently enter real timing loops to use those test seams before
running them. Ordinary test-runner elapsed-time output is not benchmark evidence.

The user launches every measurement session after disconnecting networking,
turning off Bluetooth, and closing agents and other unnecessary applications.
The agent does not remain active to monitor the campaign. Once the user returns
the output, a separate task can inspect it, prepare any pilot-backed confirmation
freeze, and later integrate adequate evidence into the paper. Neither that
follow-up nor production optimization promotion is a completion gate here.

The study follows [spec 012's evidence handoff][evidence-spec] and
[spec 013's manuscript integration][paper-spec]. It uses the existing
[benchmark workflow][workflow], [C++ adapters][adapters],
[development runner][development], and [whitepaper guide][paper-guide].
Do not introduce another campaign runner or agent-loop infrastructure.

## Review Inputs And Established Findings

The planning discussion considered three peer-review simulations and their
[archived meta-review][reviews]. Their recommendations are
research leads, not independent experimental validation or venue decisions.
The requirements below are self-contained if those local reviews are absent.

The user also supplied the chat
[Review paper benchmark reliability][reliability-chat]. Its important findings
are preserved here so execution does not depend on continued chat access:

-   The existing headline values reproduce from retained observations:
    scalar core p99 is approximately 3.42 us versus 9.50 us for the current
    vDSP adapter at N=4096, H=1024, D=64, 48 kHz, without smoothing. Aggregate
    cost is 2.27 times higher and publication is 21.3 ms later. This is evidence
    for those adapters and conditions, not an optimized-library ranking.
-   At N=16384, one hybrid throughput process costs 390.32 ns/sample while
    the other eight cost 126.40--128.94 ns/sample. The reported 157.39 mean is
    about 22 percent above the nine-process median. Retain the slow process;
    neither E-core placement nor a particular background service is established
    as its cause. See the [per-process table][process-table].
-   A [host snapshot][host-snapshot] records `sandboxd` at 90.2 percent CPU.
    It does not establish the load during each measurement. More importantly,
    the [runner][runner] builds, archives, verifies, and probes resources
    after the manually observed quiet interval. That interval therefore does
    not establish host state at the start of timing.
-   Callbacks execute back-to-back. The representative observation window
    represents 43.7 seconds of audio but only tens of milliseconds of timed
    execution. This is a useful sustained-execution microbenchmark, distinct
    from callbacks arriving every D/f_s seconds.
-   The native adapters use scalar preparation/conversion loops. The hybrid
    dispatches surrounding work per element. Stronger implementations need
    measurement before adopting a reviewer's claimed speedup from another host.
-   The confirmation campaign omits the available module backends. Those
    backends force Hann and coupled smoothing settings; their final checks
    inspect selected bins rather than audit every module output.
-   Local Rack source has per-sample worker barriers and enables ARM64
    flush-to-zero in `system::resetFpuFlags()`. Reverify against the exact Rack
    revision used by the new study. A sample period is not a separate device
    deadline, and perfect division of work by thread count is not assumed.

## Questions And Expected Behavior

1.  Does the current scheduler reduce useful host-level peaks against efficient
    native analysis, and when does its added aggregate cost outweigh that gain?
2.  How much of each result comes from scheduling, kernel choice, layout,
    preparation/output code, caching, or SIMD channel parallelism?
3.  Does a hybrid or native sub-FFT schedule provide a better cost, peak, and
    publication-age tradeoff than individual butterflies?
4.  Can a shorter completion horizon recover freshness without unacceptable
    peaks, while preserving exact frame selection and publication cadence?
5.  Do conclusions survive prepared sessions, pacing, real module work,
    configuration changes, competing DSP, and display consumption on the M1 Pro?

Behavior examples:

-   If efficient vDSP eliminates the old p99 advantage, retain that result and
    revise the claim. Do not enlarge only the workloads on which the core wins.
-   A process that is three times slower remains in raw data and the primary
    summary. A predeclared sensitivity analysis may identify it separately;
    a speed threshold alone does not prove a CPU-core assignment.
-   With H=1024 and completion horizon H_c=256, frame endpoints remain 1024
    samples apart and publication occurs 255 samples after each endpoint.
    Capture continues through the remaining hop; no extra frame is selected.
-   A silent fixture must pass with the correct silent result. A check that
    requires positive output must become fixture-aware, not be bypassed.
-   Four staggered channels may represent different input endpoints. Their
    result is labeled as an alternative with different alignment semantics,
    not a drop-in equivalent to four simultaneous channel spectra.

## Scope And Preservation

-   Preserve old campaigns, freezes, source archives, numerical policies,
    generated publication assets, and their identities. Use new backend IDs
    and new output roots where implementations or contracts change. Reanalysis
    is new derived evidence and never rewrites old raw observations.
-   Keep reusable DSP independent of Rack and provider APIs. Experimental
    backends belong in `benchmark/paper/`; host integration belongs at the Rack
    boundary. Shared kernels must not introduce allocation, locks, I/O, timers,
    logging, or drawing into production sample processing.
-   Preserve frame selection, normalization, DC/Nyquist treatment, magnitude
    smoothing, channel independence, cancellation, ownership, and saved patches.
    No parameter/port/light renumbering, slug changes, or silent JSON changes.
-   Preserve GPL attribution, C++11 production compatibility, and the existing
    C++14 benchmark/test baseline. Follow the [C++ style guide][cpp-style] when
    implementation begins. Do not modify vendored providers to improve rankings.
-   Do not add a new convolution implementation, GPU backend, automatic tuner
    in production, mailbox redesign, licensing change, or panel feature here.
    Keep existing inverse/chain results as explicitly scoped controls.
-   Worker/UI-thread analysis remains an alternative. Exact-hop analysis can
    run on a worker with suitable buffering; do not claim engine-thread
    execution is necessary or universally superior without comparing it.
-   Public deposition, submission, push, and release are separate actions.
    Prepare reproducible launch inputs locally; do not invent collected data,
    public availability, or a DOI. New measurement collection, results analysis,
    manuscript revision, venue formatting, and acceptance promises are outside
    this implementation's scope.

## Phase 1: Establish Reliable Measurement Boundaries

Primary files: [runner][runner], `benchmark/paper/protocol.hpp`,
`benchmark/paper/{execution,measurement_clock,development}.hpp`, and
`benchmarks/lib/{execution,workflow,study,check}.py` under `docs/whitepaper/`.

- [x] Define a versioned execution contract distinguishing continuous,
      absolute-time-paced, and Rack-engine runs. Record compiler flags, binary,
      source, SDK/provider identities, power mode, timer resolution, requested
      and effective thread policy, and per-thread FPU state. Unsupported policy
      requests fail or are explicitly unavailable; they never silently succeed.
- [x] Require `/usr/bin/caffeinate -is` around every macOS benchmark launch,
      including development, smoke, pilot, confirmation, and Rack-host timing.
      Hold its assertions from before stabilization through the last measured
      child process; the wrapped runner must wait for all measurement children.
      `-i` prevents idle system sleep; `-s` prevents system sleep on AC power.
      Record the wrapper command, process lifetime, and assertion evidence
      before the stabilization gate and after measurement, outside timed loops.
      Missing or prematurely ended protection fails the run's integrity check;
      retain its observations and failure record without promoting it to valid
      confirmation evidence.
- [x] Require the user's M1 Pro sessions to use AC power with Low Power Mode off.
      Verify and record power source and effective power settings separately
      from sleep assertions; flag any change during the session. `caffeinate`
      does not disable Low Power Mode, fix CPU frequency, or prevent thermal
      throttling. Fail readiness when required settings cannot be established;
      do not silently change system settings or infer stable performance from
      an active sleep assertion.
- [x] Put build, archive/hash work, full preflight, inventory verification,
      and resource probes before a recorded stabilization gate. No rebuild or
      resource probe may occur between that gate and measurement. Execute
      verified prepared binary bytes, retaining source/dependency checks.
- [x] Define session settling and inter-process preparation policies. Account
      for FFTW planning, previous correctness replay, hashing, serialization,
      and calibration work. Deliberate warmup follows the declared steady-state
      protocol; startup remains a separate cold-analysis workload. Do not assume
      a quiet interval before compilation establishes either condition.
- [x] Configure pilot launches with at least 180 seconds of settling after
      session preparation. Implement timestamped host observations and
      calibration recording; later pilot review will freeze measurable
      start/flag conditions. Absence of a thermal warning is not a temperature
      or frequency measurement. Bound gate waiting and retain failure/timeout
      evidence without killing services or repeatedly restarting until a
      favorable run appears.
- [x] Support unattended serial execution without active interactive
      agent/editor work, builds, tests, or reporting during the campaign.
      Require fully local launch inputs and no network access, downloads,
      dependency resolution, remote logging, or agent connection. Document the
      user's manual preparation: networking disconnected, Bluetooth off, agents
      stopped, and unnecessary applications closed. Record verified state and
      user declarations separately; disconnected radios alone do not prove an
      idle host. Do not change those settings or terminate processes on behalf
      of the user.
- [x] For paced runs, retain scheduled release, actual start, and finish times;
      distinguish compute duration, wake-up lateness, and release-to-finish
      deadline misses. Use absolute deadlines and an explicit overrun policy:
      preserve logical sample order, never silently drop or rebase late work.
      Label catch-up execution. Include deliberately interleaved DSP in timing;
      keep synthetic cache-conditioning cost separately identified.
- [x] Implement configurable throughput chunks with original ordering retained.
      Prepare explicit initial duration, warmup, calibration-placement, and
      pacing policies for later pilot review. Do not subtract timer overhead
      or time every butterfly in the primary loop.
- [x] Record pre/post calibration and available host-state evidence. Keep all
      slow runs. A predeclared flag policy supports sensitivity analysis, not
      retrospective deletion or unsupported E-core/preemption attribution.
- [x] Add runner tests proving preparation precedes settling, measured binary
      identity cannot change, required sleep assertions cover the campaign,
      unsupported policies are visible, and failed or partial runs cannot
      become valid confirmation evidence. Test pacing arithmetic/overruns with
      deterministic clock fixtures separately from hardware smoke execution.

Gate: untimed checks and fake-clock/stub-process tests verify sleep protection,
power checks, pacing, stabilization order, and complete failure records. The
user's smoke/pilot launch is prepared; no real timing run is needed to pass.

## Phase 2: Workload Controls, Metrics, And Audits

Primary files: `benchmark/paper/{protocol,backend,analysis_reference}.hpp`,
`docs/whitepaper/benchmarks/lib/{profiles,contracts,observations,report}.py`,
the backend registry, and their existing verifier/test directories.

- [x] Add explicit window, independent octave/temporal smoothing, fixture/seed,
      active-port/voice, execution-regime, and experimental-policy fields.
      Specify temporal smoothing in samples/alpha or physical time explicitly;
      preserve the modules' time-control convention. Include every effective
      field in C++/Python contracts, hashes, freezes, raw metadata, and grouping.
      Existing protocol versions keep their historical meanings.
- [x] Add silence, signal-to-silence decay, impulse, DC/Nyquist, off-bin tones,
      weak signals, deterministic noise, and independent-channel fixtures.
      State supported finite input ranges and FPU-dependent expectations.
      Retain independent all-bin reference audits and existing error budgets;
      numerical changes do not justify loosening tolerances after timing.
- [x] Implement per-hop maximum-callback distributions, per-phase profiles,
      peak/mean summaries, individual process points, means and medians,
      sample counts, elapsed duration, and actual budget-exceedance counts.
      Keep p99 and maxima as secondary descriptive outcomes.
- [x] Define hop grouping using actual frame endpoints and callback intervals,
      including offsets, staggered instances, partial edge hops, D>H, and
      non-divisible H/D. A callback overlapping multiple hops cannot yield
      independent hop observations or a fabricated sub-callback duration.
      Test these cases with synthetic traces and retain original coordinates.
- [x] Separate per-analyzer, aggregate-block, publication, consumer, and device
      metrics. Fractional-budget thresholds are explicit allocation scenarios;
      they are not actual device deadlines. Do not pool channels or callbacks
      as independent process/session replicates. Any confidence intervals must
      respect temporal clustering and state their limited same-host scope.
- [x] Use retained raw evidence as regression fixtures for headline values,
      hop peaks, phase imbalance, slow hybrid processes, maxima, and exceedances.
      Keep inverse/chain misses in their own comparison boundary. New research
      interpretation belongs to the follow-up after user measurements.
- [x] Allow small explicit-workload profiles without appending an entire
      preset. Revise fast development coverage to include available macOS
      native analysis when enabled, SIMD, and representative module work;
      keep optional-provider omissions explicit. Keep development and frozen
      publication repetition policies distinct. Phase 3 adds efficient native
      replacements to this coverage; the existing adapter is not relabeled as
      an optimized baseline.
- [x] Extend report/export/bundle checks to the new metrics and schemas.
      Demonstrate that malformed timestamps, missing bins, missing processes,
      invalid norms, and mixed contracts fail. Historical archives remain
      readable by their recorded schema/tooling.

Gate: a fixture report demonstrates all metrics and audit failures, and the
old checked publication assets still reproduce unchanged.

## Phase 3: Credible Baselines And Matched Controls

Primary files: `benchmark/paper/{native_kernels,native_analysis,native_diagnostics}.hpp`,
the existing provider/control headers, and the narrow
`SpectrumAnalysis::process_scheduled` placement seam.

- [x] Add efficient native analysis as new backends. Use two-span or otherwise
      efficient retained-input access, provider-appropriate window/packing,
      conversion, magnitude, and output kernels. Preserve mathematical scaling
      and audit floating-point differences independently. Inspect generated
      code or retained profiles when choosing variants. Defer claims about
      their actual cost until the user supplies measurements.
- [x] For PFFFT, evaluate unordered output only with correct natural-frequency
      mapping wherever bands or display bins require it. Charge every necessary
      reorder/conversion/store to the declared boundary. Retain the existing
      portable scalar-glue adapters as historical controls.
- [x] Add batched independent four-channel native analysis where supported,
      and per-channel/instance staggering. Record frame endpoints per channel;
      distinguish simultaneous spectra from staggered freshness. Equal channel
      counts alone do not establish equivalent output semantics.
- [x] Add an efficient hybrid with contiguous stage-segment dispatch and a
      matched batch mode sharing its kernels and storage. Retain the old hybrid
      pair so later measurements can test its per-element dispatch overhead.
- [x] Add a matched batch/distributed pair using the current core's arithmetic,
      layout, cache policy, and output contract. Existing legacy controls also
      change layout and boundary work, so do not substitute them for this pair.
      Factor only the needed execution seam; avoid a general scheduler framework
      or a copied production implementation that can silently diverge.
- [x] Add diagnostic stage-cost measurements and untimed operation traces.
      Prepare instrumentation-overhead controls; primary measurement paths
      retain uninstrumented timing boundaries. Defer running these diagnostics
      to the user, and do not sum stage times as if they reproduce integrated
      cost.
- [x] Verify every new backend with all-output references, frame-retention and
      publication checks, cold/live cache cases, allocation probes, and explicit
      opaque-provider limitations before its first performance pilot.

Gate: stronger native and matched scheduling controls produce correct outputs
under the same declared tasks. No required baseline is omitted for being faster.

## Phase 4: Modules, Rack, And Concurrent Consumers

Primary files: [module adapters][modules], `benchmark/paper/benchmark.cpp`,
benchmark-only Rack-engine integration, and `test/paper/`/`test/rack/` verifiers.

- [x] Add explicit shipped-default and controlled-comparison module modes.
      Record actual quantized values after applying controls. Fourier defaults
      include four lanes, N=2048, a nominal 30 ms hop, and Flattop; Spectre
      has N=2048/H=1024. Cover Fourier N=16384 with the nominal 5 ms hop.
      Distinguish active ports from polyphonic voices summed into each port.
- [x] Replace selected-bin module checks with an untimed all-output audit of
      input normalization, DC blocking/gain, spectra, smoothing, coordinate
      mapping, and publication. Use independent expectations where possible;
      checking production output against the same production calculation is
      insufficient. Keep timing free of reference work and audit polling.
- [x] Build a headless harness using the actual pinned Rack engine. Include
      current modules and comparable benchmark-only native/hybrid analyzer
      modules with matched conditioning, display preparation, and publication.
      A trivial-sink native analyzer cannot stand in for a complete module.
- [x] Prepare cases with 1/4 engine threads, 1/4/16 analyzers, D=64/256, 48 kHz,
      aligned/staggered phases, and a fixed background-DSP workload sweep.
      D=16 or higher-rate host stress extensions may follow pilot review in a
      later task. Specify identical background workloads across contenders;
      do not equalize total utilization separately and hide extra analyzer cost.
- [x] Implement recording of full engine-block duration, aggregate CPU cost,
      hop peaks, publication age, and budget misses. An analyzer-free engine is
      a useful overhead control, not a value automatically subtracted from
      observations. Verify engine barriers, worker policy, lifecycle locking,
      and FPU mode against retained Rack source. Do not assume ideal
      cost/thread-count scaling.
- [x] Prepare concurrent single-consumer workloads at representative 30/60 Hz
      rates, with deliberate stalls, and recording for producer impact,
      consumed-spectrum age, skipped updates, and snapshot consistency.
      Validate ownership and snapshot correctness untimed. Preallocate storage;
      do not add a second mailbox consumer or synchronize unsafely through the
      module's other fields. Separate headless consumption from actual rendering.
- [x] Add controlled reset, freeze/resume, sample-rate, window, band, and geometry
      transitions. Separate lifecycle allocations and host-lock stalls from
      steady sample processing. Include silence decay under recorded FPU modes.
      Verify single-producer ownership during host-serialized reset/publication.
- [x] Use deterministic concurrency tests and the existing mailbox TSan check
      outside timing. An eventual device-underrun claim additionally requires
      a user-run audio-device experiment with recorded device/driver/buffer
      policy and overload telemetry. Device experiments are a later extension;
      headless exceedances alone support no audible claim.

Gate: complete modules and pinned Rack-engine workloads pass untimed correctness
and ownership checks at defaults and demanding settings. Recording paths are
tested using fixtures; host performance measurement remains user-owned.

## Phase 5: Prepare Scheduling And Kernel Experiments

These are benchmark-only candidates. Implement and numerically validate the
following in order; record readiness or a reasoned deferral for each. Performance
evaluation, weight tuning, tradeoff curves, and winner selection happen after
the user returns measurements. Missing candidates limit later claims.

- [x] Native sub-FFT scheduling: bounded native leaves, scheduled twiddles,
      permutations, reconstruction, and postprocessing. Include all copies and
      scratch costs; an opaque leaf is indivisible. Prepare a small predeclared
      leaf-size sweep against whole-native-FFT hybrid and butterfly scheduling
      without assuming either wins.
- [x] Completion horizon: decouple H_c from H; test H/4, H/2, H, and immediate
      execution where supported. Define rounding for non-divisible H and
      1 <= H_c <= H. Preserve endpoint cadence, capture during idle phases,
      latching, cancellation, ownership, and publication at t_r+H_c-1.
      Update C++/Python delay contracts and ring-lifetime reasoning. Merely
      changing the quota denominator is not a complete implementation.
- [x] Stage weights: expose preparation, reconstruction/magnitude, output,
      and dirty-band weight sweeps with scalar/SIMD and real module sinks.
      Disclose earlier M1 Pro tuning from [spec 010][weight-spec]. Specify
      tuning and held-out sizes, hops, and states before measurement. Defer
      tuning decisions; observed p99 will not establish a WCET bound.
- [x] Kernel variants: use source inspection or existing profiles to prepare
      ring-index arithmetic, scalar butterfly segmentation,
      magnitude/reconstruction, and coordinate-mapping experiments separately.
      Do not convert magnitude smoothing to power smoothing, change
      coherent gain, or add signal-dependent shortcuts without a distinct
      contract. Capture before/after builds and test each change independently.
- [x] Extend deterministic tests for zero-work calls, quotas crossing stages,
      sparse/dense paths, wraparound, H=1, H>N, changing H/H_c, all windows,
      DC/Nyquist, independent lanes, reset mid-cache-rebuild, and unchanged
      numerical/publication behavior where the contract promises it.
- [x] Keep credit and time models separate. The reviewer-proposed bound
      `c0 + kappa * ceil(W/H)` does not follow from weights when an entire
      weighted operation executes on its first credit. For dirty N=4, H=16,
      o=1, W=15, a one-credit call can execute a weight-four operation. Any
      replacement cost model must account for indivisible boundary operations,
      dispatch/configuration overhead, and its explicit cost assumptions.
      Prepare held-out workloads for later validation of predictions without
      calling them WCET.

Gate: prepared candidates pass numerical/scheduling checks, deferred candidates
are documented, and the user can collect every planned comparison. No timing
result or optimization selection is required.

## Phase 6: Prepare The M1 Pro Study Plan

The primary host is the existing 10-core, 16 GiB M1 Pro. The prepared package
must capture actual OS, compiler, SDK, and provider identities; the user-run
launcher will capture current power and scheduling state. Do not inherit the
old campaign's software versions or host preparation claims.

Prepare the following focused groups rather than a full Cartesian product:

| Group | Initial Pilot Coverage | Main Question |
| --- | --- | --- |
| Baselines | N=2048/4096/16384, H=1024, D=64, scalar unsmoothed, core/matched/native/hybrid | Do efficient baselines change the original result? |
| Granularity | Selected N/H, leaf size, H_c, and weights; scalar and independent four-channel | Which granularity and delay meet the requirement? |
| Scaling | D=16/64/256, fixed H and separately fixed H/N, supported precision | Which scaling relationship explains the result? |
| Modules | Actual defaults, extreme Fourier settings, independent smoothing and active ports | Does the complete shipped path behave like the core? |
| Host | Phase 4 thread/count/block/load/consumer cases | Does reducing bursts improve available host capacity? |
| Stress | Non-divisible hops, cold/live caches, silence decay, lifecycle events | Which limits or regressions constrain the recommendation? |

- [x] Write hypotheses, primary outcomes, initial observation lengths, process
      repetitions, allowed session order seeds, flagging policy, aggregation,
      and stopping rules into explicit versioned profiles. Prepare candidate
      practical thresholds in absolute time, miss-rate or sustainable fixed-load
      terms, together with spectrum-age/cost budgets. These are study-design
      inputs, not conclusions about expected performance.
- [x] Prepare commands for at least two separately prepared user-run pilot
      sessions covering all required groups. Resolve implementation errors
      using correctness tests before handoff. Preserve all attempts; pilots
      will later inform drift, multimodality, resolution, phase coverage,
      duration, and storage decisions. No pilot run is required to close this
      preparation spec.
- [x] Preserve the pilot-backed confirmation boundary. The initial package
      collects pilots; it must not fabricate a confirmation freeze without
      measurements or automatically promote pilot observations. After the user
      returns pilots, a separate analysis task can propose a freeze and supply
      confirmation commands for the user. Plan for at least three confirmation
      sessions on separate days and five fresh processes per primary cell per
      session, subject to the later pilot review. That decision and those runs
      are outside this spec's completion requirements.
- [x] Extend freeze validation to support a predeclared, recorded session-seed
      policy while preserving workload membership and measurement semantics.
      Test it against synthetic pilot records. The existing fixed-seed freeze
      must not be bypassed, and fixture data must never qualify as real evidence.
- [x] Make observation lengths configurable and retain effective hop counts,
      duration, quantile ranks, and process/session variation. Test summaries
      and inadequate-sample reporting using fixtures; hundreds of hops do not
      establish a precise rare-event probability.
- [x] Enforce serial measurement of unchanged prepared artifacts. Changes to
      source, dependencies, profiles, or instrumentation invalidate the relevant
      preparation and any later freeze. Implement clear errors and new-output
      requirements instead of silently rebuilding or relabeling observations.
- [x] Preserve every planned cell and slow observation. Implement completeness
      and integrity checks that distinguish a failed run from a disturbed but
      valid observation. No best-run selection, winner-based early stopping,
      silent omissions, or performance-driven automatic retries.

Gate: checked profiles, resolved workloads, immutable prepared identities, and
fixture-tested recording policies are ready for user collection. No measured
pilot, confirmation freeze, or statistical result is required.

## Phase 7: Offline Launch Package And User Handoff

Primary files: the existing Make/benchmark entry points, workflow library,
benchmark profiles, and their tests/documentation. Extend those components;
the convenience targets below must not introduce a second campaign engine.

- [x] Add `make benchmark-study-prepare` to prepare all required groups in one
      operation. Resolve dependencies while connectivity is available, build
      executables, run untimed correctness checks, archive source/dependency
      identities, and save an explicit launch manifest. Preparation never
      invokes a timing pass, calibration benchmark, or implicit smoke campaign.
      Record unavailable providers as errors for required comparisons.
- [x] Add `make benchmark-study-run SESSION=pilot-01` as the user entry point.
      Resolve groups, profiles, binary paths, host identity, order seed, output
      paths, and recording policy from the prepared manifest. Require an
      explicit, fresh session label. The target must have no build prerequisite
      and no network operation; absent, stale, or incompatible preparation is
      an actionable failure, never an automatic rebuild or dependency download.
- [x] Have that launch apply `/usr/bin/caffeinate -is` automatically, retain its
      assertions for the entire runner and its children, check readiness,
      perform the declared stabilization wait, and execute groups serially.
      Any expensive verification precedes settling. Keep deliberate per-process
      setup and warmup within the declared Phase 1 policy. The user must not
      need to assemble flags, manually time a sleep interval, or keep an agent
      connected. Do not create a recurring task or remote monitor.
- [x] Keep campaign outputs outside normal clean/build directories, defaulting
      to `~/Fourier-benchmarks/spec014/SESSION/`. Expand and print the actual
      absolute location. Retain raw observations, complete/failed status,
      prepared manifest, workload/seed policy, checksums, numerical audits,
      logs, and source/dependency identities needed for later analysis. Preserve
      failed/interrupted directories and reject collisions; never delete or
      overwrite an earlier session to retry it.
- [x] Provide a short quiet-host checklist: connect AC, disable Low Power Mode,
      keep the lid open, disconnect Wi-Fi/Ethernet/other network connections,
      turn off Bluetooth, stop agents, and close unnecessary applications.
      Use the built-in keyboard/trackpad or wired controls as needed. Launch
      from a standalone terminal after closing Codex and other agent hosts,
      then leave the machine alone. Record user-declared isolation separately
      from observable host state, and never claim that all OS activity ceased.
      The launcher must not toggle radios, modify power settings, kill services,
      or require the internet to validate readiness.
- [x] Print local progress/status at boundaries outside measured loops and a
      final result-directory path. Once timing ends, write an offline handback
      archive with its checksum and a concise README. Include everything needed
      for the follow-up except documented restricted SDK/provider binaries.
      Do not render reports, select favorable rows, rewrite the paper, or start
      another independent session automatically. No upload is required.
- [x] Add `make benchmark-study-check` to test launch dispatch, stale-input
      rejection, sleep-assertion lifetime, quiet-gate ordering, offline operation,
      argument forwarding, output collisions, interrupted children, and final
      packaging using fake clocks, fixture records, and stub measurement
      processes. Untimed real numerical/concurrency checks remain separate.
      Distinguish synthetic artifacts unmistakably from measured campaigns.
- [x] With dependencies already available locally, validate preparation and
      fixture-based launch checks with network calls blocked and no connected
      agent service required. Confirm that missing dependencies produce
      preparation instructions, not a fetch during launch. Document any untested
      real-host measurement behavior; obtaining actual performance numbers is
      the user's task.
- [x] Deliver the exact repository path, successful preparation/check commands,
      manifest identity, copyable session launches, expected output/archive
      paths, completion/failure markers, and interruption instructions. Provide
      workload counts and labeled planning estimates; do not invent a measured
      runtime. Tell the user which archive to return once agents can be reopened.

Gate: the user can disconnect, close agents, and launch each independent session
with one short command against a fully prepared local package. All preparation
and untimed/fixture checks pass. Archive this spec as COMPLETE at that handoff,
recording any deliberate candidate deferrals and unverified timing behavior;
do not wait for measurements, interpretation, paper changes, or publication.

## Validation And Final Launch Commands

Run commands from the repository root. The [contributor guide][contributing]
and [workflow][workflow] specify the Rack SDK, compiler, optional providers,
Python, and existing prerequisites. During implementation, use only correctness
and fixture checks: no real benchmark run is authorized by this spec. Audit
any changed test path before running it; replace entry into real timing loops
with a fake clock or stub process. Do not weaken numerical checks to avoid work.

### Existing Preparation Checks

```shell
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests -p 'test_*.py'
make test-benchmark-dev
make test/dsp/test_spectrum_analysis
make test
make test-mailbox INSTRUMENT=tsan
make test-rack
make -j2
python3 docs/whitepaper/tools/comparison_paper.py --check
make -C docs/whitepaper check
git diff --check
```

Run `make test/dsp/test_spectrum_analysis INSTRUMENT=asan-ubsan` for relevant
memory/lifecycle changes. Provider skips are reported, not counted as passes;
required macOS providers must be available in the prepared package. A passing
DSP test is not a successful Rack build, manual session, or benchmark. These
are future implementation commands, not claims of execution while editing the
spec. Documentation-only changes use link, command, and diff checks.

### Required Simple Command Interface

The following Make targets are implemented and verified. Exact profile filenames,
resolved workload lists, paths and source identities are retained in the generated
manifest and the offline handoff guide so the user's commands remain short.

The agent prepares the package while dependencies and build tools are available:

```shell
make benchmark-study-prepare
make benchmark-study-check
```

The final handoff must give the absolute repository path and instruct the user
to open a standalone terminal there, apply the quiet-host checklist, and launch:

```shell
make benchmark-study-run SESSION=pilot-01
```

After that session finishes, in a separate prepared session:

```shell
make benchmark-study-run SESSION=pilot-02
```

Each command covers all required pilot groups from the prepared manifest and
applies `caffeinate -is` internally. These examples are user commands, never
agent validation steps. The final handoff should contain no long per-backend
CLI sequence, dependency installation, plotting step, or manual profile edits.
The user can reconnect and reopen an agent after collection to return the
printed archives. A second independently prepared session must not be simulated
by looping over two labels in the same launch.

### Follow-Up After The User Returns Results

Separate future work will check the returned evidence, assess pilot stability,
choose practical thresholds and adequate durations, and prepare any required
confirmation freeze and user launch commands. After sufficient evidence is
returned, it can compare cost/peak/age tradeoffs, retain unfavorable results,
produce checked figures/tables and portable evidence, revise the full paper,
and decide whether a verified optimization merits production integration.
Those tasks must not run automatically as part of this preparation spec.

### Completion Checklist

- [x] Phase 1: sleep protection, power/isolation checks, stabilization, pacing,
      and failure behavior verified with untimed checks and fixtures.
- [x] Phase 2: explicit workloads, metrics, compatibility, and audits verified.
- [x] Phase 3: native and matched scheduling controls numerically validated.
- [x] Phase 4: module, Rack-engine, consumer, and stress workloads prepared and
      correctness/ownership checks passed.
- [x] Phase 5: experimental variants prepared or explicitly deferred; no
      measured performance selection required.
- [x] Phase 6: pilot profiles and future confirmation policy prepared and
      fixture-validated; no campaign executed by the agent.
- [x] Phase 7: offline package, simple launch targets, handback format, and
      exact user commands delivered; no paper integration required.

## Execution Evidence

September 30, 2026: specification created from the paper-review discussion,
repository inspection, and the linked reliability review. No implementation,
new performance campaign, production change, or manuscript rewrite is claimed.
Spec-author validation passed `make -C docs/whitepaper check`, repository-link
checks, shell syntax checks, and argument parsing of all 18 benchmark CLI
examples without executing the planned campaigns. Reviewed the specification
against existing source, recorded observations, and contributor guidance.

September 30, 2026: added mandatory macOS `caffeinate -is` protection, AC-power
and Low Power Mode requirements, assertion-lifetime checks, and wrapped launch
examples. Verified option semantics against the installed `caffeinate(8)` and
`pmset(1)` manuals. Repository links, shell syntax, all 18 benchmark CLI examples
(including four wrapped launches), and `git diff --check` passed. No benchmark
campaign or machine power-setting change was performed for this documentation
update.

September 30, 2026: narrowed implementation to benchmark preparation and a
user-run offline handoff. Replaced measurement, confirmation-result, and paper
completion gates with untimed/fixture validation and planned simple Make
targets. Retained mandatory `caffeinate`, power checks, settling, and data
integrity requirements. Repository links, shell-example syntax, existing
command references, and `git diff --check` passed. The new Make targets remain
unimplemented requirements; no benchmark was launched or code changed.

### Phase 1 Implementation: September 30, 2026

The preparation-only specification was committed as `1892663` before Phase 1
implementation. The measurement-boundary code now provides:

-   Execution contract version 1, continuous and absolute-time-paced streaming,
    ordered throughput chunks, and calibration/thread/FPU sidecars. Paced
    overruns catch up without dropping or rebasing samples; conditioning remains
    separate from compute duration. Frozen comparisons and report strata include
    execution policy; historical archives remain readable.
-   A local macOS guard for campaign, native development, and standalone Make
    launches. It owns and verifies both `caffeinate -is` assertions, checks AC
    power and Low Power Mode separately, and retains readiness failures.
    Failed guards invalidate their own development output without modifying a
    colliding prior run. Numerical tests and help/list/build targets are exempt.
-   A recorded session gate after heavy preparation, with 180 seconds of
    settling by default; plans precede a further 1000 ms per-workload interval
    and deliberate warmup. Publication runs execute their retained binary.
    Native development settles after its in-process preflight. Partial
    observations and failed status survive ordinary processing/guard failures.
-   Deterministic clock fixtures and stub launches covering release arithmetic,
    overruns, sample ordering, preparation order, guard lifetime, power failures,
    binary replacement, sidecar integrity, and interrupted/failed evidence.
    Synthetic sidecars cannot qualify as measurements. Existing correctness
    fixtures that entered timing loops now use a synthetic clock.

Validation actually run from the repository root:

```shell
python3 -m unittest discover -v -s docs/whitepaper/benchmarks/tests -p 'test_*.py'
python3 scripts/test-build.py
make test-benchmark-dev PAPER_VDSP=1 PAPER_FFTW_PREFIX=
make -j2 benchmark-paper-build PAPER_VDSP=1 PAPER_FFTW_PREFIX=
DYLD_LIBRARY_PATH=/Users/christiankauten/Documents/Projects/Rack LD_LIBRARY_PATH=/Users/christiankauten/Documents/Projects/Rack .build/benchmark/rack/paper --verify
make -C docs/whitepaper check
git diff --check
```

Results: the Python suite passed (93 tests, one optional plotting-environment skip);
five Make-dispatch fixture tests passed; native development tests passed 127
assertions in seven cases. Both benchmark executables built with PFFFT/vDSP;
the compiled `--inventory` matched the Python registry with `features=['vdsp']`.
Untimed native preflight passed. The Python suite also exercised the available
FFTW adapter fixtures; the native build above deliberately disabled FFTW.
Existing Rack SDK deprecation warnings remain. Whitepaper checks verified
historical campaign/source hashes, 2880 timing rows, 27 phase rows, 31 references,
numerical tables, figures, and 32768 schedules. Documentation links, shell
syntax, and whitespace checks passed.

Limitations: no performance pass, live `caffeinate` launch, paced hardware run,
full plugin build, or manual Rack session was performed. Live power readiness
remains unverified: this agent environment's read-only `pmset -g` response did
not expose Low Power Mode, which the launcher correctly treats as unreadable
and would reject. Pre/post host snapshots cannot exclude transient power or
thermal changes; user isolation notes do not prove an idle host. Only the
calling thread's effective scheduler/QoS/FPU state is observable here; opaque
provider worker state is explicitly unavailable. Unsupported thread/FPU policies
and the future Rack-engine regime fail explicitly.

At Phase 1 completion, Phases 2--7 remained pending, including
the prepared no-build launcher, isolated-host checklist/manifest, and final
one-command handoff. Existing measurement commands still perform preparation
before settling; they are not the promised Phase 7 offline package. Do not
archive this spec or launch measurements to close the remaining preparation.

### Phase 2 Implementation: September 30, 2026

Committed Phase 1 as `29111a6` before implementing Phase 2. The new benchmark
code and [workload guide](../../docs/whitepaper/benchmarks/guides/workloads.md)
provide:

-   Opt-in v3 contracts with independent window/octave/temporal settings,
    deterministic input/seed policy, active ports/voices, execution regime,
    and explicit experimental-policy rejection. C++ and Python retain requested
    controls and effective binary32 smoothing values. All fields participate
    in identity, freeze enforcement, metadata, grouping, and report tables.
    Module time smoothing preserves the panel's exp(-10 H/f_s/seconds)
    convention and public 0--2.5 second range. Old protocols remain unchanged.
-   Silence, one-shot decay/impulse, DC/Nyquist, off-bin/weak tones, seeded
    noise, and independent channels. Input preparation and alpha conversion
    happen before measured sample work. Existing norm budgets remain intact.
    Untimed tests cover 108 signal/window/smoothing combinations across scalar,
    legacy and PFFFT adapters, plus independent scalar/SIMD channels and real
    module control/port/voice wiring. Silence is valid; logarithmic negative
    infinity is accepted only as a valid Fourier display y coordinate.
-   Coordinate-preserving hop peaks, callback phase distributions, peak/mean,
    process points, session means/medians, durations/counts, and actual
    allocation-budget exceedances. Six new CSV tables and checked JSON retain
    partial/shared hops, non-divisible H/D, offsets/staggering, and D>H without
    splitting callbacks or creating independent replicates. Paced release
    misses remain distinct from compute scenarios and unavailable device data.
-   Five compact retained raw fixtures, with original hashes and metadata,
    reproducing maxima, hop peaks, phase imbalance, the slow hybrid process,
    and an inverse process's 64 compute exceedances. Nine-process historical
    summaries still reproduce the 3.42/9.50 us and 2.27 headline values. This
    reads old measurements; no new performance observations were collected.
-   Small schema-3 explicit profiles with no appended preset. Explicit module
    requests survive the new profile's provider filter; old matrices remain
    intact. Fast development now covers 36 workloads, or 40 with vDSP enabled,
    including independent SIMD and both modules. Missing vDSP is reported.
    Current native adapters remain the existing baselines; efficient versions
    and their inclusion belong to Phase 3.
-   Synthetic v3 campaign/report/export/bundle round trips and rejection of
    corrupted timestamps, missing bins/processes/instances, invalid norms,
    changed controls/freezes, wrong schemas, edited scheduling JSON, and edited
    derived tables even after rehashing. Synthetic examples are never promoted
    to real confirmation evidence.

Validation actually run from the repository root:

```shell
python3 -m unittest discover -v -s docs/whitepaper/benchmarks/tests -p 'test_*.py'
python3 -m unittest discover -v -s docs/whitepaper/benchmarks/tests -p 'test_scheduling_metrics.py'
make test-benchmark-dev PAPER_VDSP=1 PAPER_FFTW_PREFIX=
make -j2 benchmark-paper-build PAPER_VDSP=1 PAPER_FFTW_PREFIX=
DYLD_LIBRARY_PATH=../.. LD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --verify
DYLD_LIBRARY_PATH=../.. LD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --development --profile fast --list
python3 docs/whitepaper/benchmarks/bench.py plan --profile controls --variant rack --output .build/controls-plan-phase2.json
python3 docs/whitepaper/benchmarks/bench.py plan --profile controls --variant macos --output .build/controls-macos-plan-phase2.json
make -C docs/whitepaper check
python3 docs/whitepaper/tools/comparison_paper.py --check
git diff --check
```

Results: 105 Python tests passed with one optional plotting-environment skip;
the seven scheduling tests also passed after the final evidence-integrity
change. Native development tests passed 196 assertions in eight cases. Both
benchmark executables built with PFFFT/vDSP and no FFTW, and untimed native
preflight passed. A separate inspection compared 72 native `--describe`
contracts with Python exactly; the persistent fixture suite also checks C++
JSON/contract parity. The controls plans retain five/six workloads respectively,
including both modules. Whitepaper checks passed the historical hashes,
2880 timing rows, 27 phase rows, 31 references, numerical tables/figures,
32768 schedules, and byte-for-byte reproduction of all nine editorial assets.
Repository links, new shell-example syntax, and whitespace checks passed.

Limitations: no performance pass, hardware smoke, live sleep/power guard,
full plugin build, or manual Rack session ran. No production source or paper
result changed. Existing Rack SDK deprecation warnings and an unused window
name-helper warning remain. Float input is bounded; underflow/flush-to-zero
behavior remains FPU-policy dependent. Module checks are control/display
audits, not Phase 4's full module oracle. There are no new consumer, Rack
engine, device deadline, or optimized native results. Phase 2's fixture/untimed
gate is complete; Phases 3--7 and the final no-build offline launcher remain
pending. The spec remains IN PROGRESS.

### Phase 3 Implementation: September 30, 2026

Committed Phase 2 as `0ce591c` before implementing Phase 3. The
[native baseline guide](../../docs/whitepaper/benchmarks/guides/baselines.md)
documents 24 additional backend identities and their comparison boundaries:

-   Matched current-core batch/distributed controls in float/double use one
    production implementation, with identical arithmetic order, storage,
    dirty-cache policy and output stores. The existing `process()` API retains
    distributed placement; the experiment selects a separate compile-time
    `process_scheduled` policy. Bitwise equality holds across the tested
    cold/live, small/large hop and retained-frame cases. No production
    optimization has been promoted on the strength of an untimed check.
-   PFFFT, vDSP and FFTW native batch/hybrid pipelines use retained contiguous
    input spans, direct native layouts, segment dispatch, cached bands/windows
    and natural-frequency output. PFFFT unordered execution explicitly charges
    reorder work and its extra buffer. Existing scalar-glue and per-element
    hybrid identities remain unchanged. Old/new hybrid cache policy also
    differs; their comparison must not be called a pure dispatch ablation.
-   vDSP and FFTW provide true four-channel batched real transforms in both
    precisions. Independent fixtures and every channel's outputs are checked.
    Existing instance staggering also applies to the new pipelines; one-channel
    instances consume offset copies of the common fixture, while native4 keeps
    simultaneous endpoints within each instance. These freshness/input
    differences remain explicit rather than treating equal channel counts as
    a matched comparison. PFFFT has no new batched API here.
-   Native replay checks every publication and bin, retaining per-instance
    and per-channel endpoints/counts. Provider layout/batching metadata and
    the spectrum-norm policy are required during archive checking; reports,
    exports and bundles retain the new coverage. C++ allocation probes pass
    during sample processing, including cold/live cases. Native allocator,
    worker, setup and scratch behavior remains explicitly opaque.
-   Separate `trace`, `stages` and `overhead` diagnostics retain stage units,
    sample coordinates, exact configuration and independent numerical replay.
    The primary pipeline contains no stage timers/trace storage. Trace reads
    no clock; timed diagnostics require the existing macOS sleep/power guard
    and a separate execution sidecar. Synthetic-clock identity and malformed
    stage placement/counts are checked. Instrumented costs exclude some
    ingestion/scheduler work and must not be summed into an integrated cost.
-   Fast development includes the matched core/PFFFT pairs and optional
    native vDSP/FFTW batch cases: 52 workloads with Rack/PFFFT, 60 with vDSP,
    or 64 with both optional providers. Full development includes all new
    identities (218 Rack/PFFFT cases, 264 with both providers). Campaign pilot
    selections and the final offline package remain Phases 6 and 7.

Validation actually run from the repository root:

```shell
python3 -m unittest discover -v -s docs/whitepaper/benchmarks/tests -p 'test_*.py'
python3 -m unittest discover -v -s docs/whitepaper/benchmarks/tests -p 'test_native.py'
CXX='clang++ -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' python3 -m unittest discover -v -s docs/whitepaper/benchmarks/tests -p 'test_native.py'
make test-benchmark-dev PAPER_VDSP=1 PAPER_FFTW_PREFIX=.build/deps/fftw
make test/dsp/test_spectrum_analysis
make test/dsp/test_spectrum_analysis INSTRUMENT=asan-ubsan
make -j2 benchmark-paper-build PAPER_VDSP=1 PAPER_FFTW_PREFIX=.build/deps/fftw
make -j2
DYLD_LIBRARY_PATH=../.. LD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --verify
DYLD_LIBRARY_PATH=../.. LD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --inventory
DYLD_LIBRARY_PATH=../.. LD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --development --profile fast --list
DYLD_LIBRARY_PATH=../.. LD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --development --profile full --list
make -C docs/whitepaper check
python3 docs/whitepaper/tools/comparison_paper.py --check
git diff --check
```

Results: 109 Python tests passed with one optional plotting-environment skip.
The four native tests also passed after the final kernel/diagnostic changes,
both normally and with ASan/UBSan.
They cover all ten provider/precision/channel kernel combinations, N up to
16384, H=1/37/257/65536, explicit signal fixtures, startup/live caches, ring
retention, all-output references, zero observed C++ processing allocations,
and matched-core equality. Native development passed 272 assertions in eight
cases. The core DSP regression passed 2,180,346 assertions in 13 cases both
normally and with ASan/UBSan. Both benchmark executables and the Rack plugin
built; full untimed preflight passed with PFFFT, vDSP and FFTW enabled.

A separate inspection matched the compiled inventory and all 24 new v3
`--describe` contracts against Python. The actual `--diagnostic trace` path
also passed structural and numerical validation, without a clock; its local
artifact is `.build/native-trace-phase3.json`. Fake-clock tests checked timed
diagnostics and their execution sidecar, never actual stage costs. A synthetic
native campaign passed check/report/export/bundle round trips; missing native
coverage, altered endpoints/counts and malformed stage records were rejected.
Whitepaper checks reproduced the historical campaign/source hashes, 2880 timing
rows, 27 phase rows, 31 references, 32768 schedules and nine editorial assets.
Documentation links and whitespace checks passed.

Generated-code inspection used Apple Clang 21.0.0 (`clang-2100.1.1.101`) for
arm64 with C++11, `-O3 -funsafe-math-optimizations`, `-Rpass=loop-vectorize`
and assembly output. Focused instantiations of `native_window<float/double>`
and PFFFT native magnitudes emitted four-float/two-double window vectors and
four-float magnitude vectors, including `fmul.4s`, `fmul.2d` and `fsqrt.4s`.
The linked binary references vDSP strided window, magnitude and batched-real
APIs, and includes both FFTW real plan-many entry points. These observations
justify trying the implementations; they establish no measured speedup.

Limitations: no performance pass, hardware smoke, real stage/overhead timing,
live sleep/power guard or manual Rack session ran. No paper result changed.
Existing Rack deprecation and unused window-name-helper warnings remain.
The installed FFTW archives target macOS 26 while the benchmark link target
is macOS 11, producing deployment warnings; this host build is verified, but
older-macOS portability is not. Native execution allocations and provider
internals remain outside the C++ audit. Phase 3's untimed gate is complete;
Phases 4--7 remain pending and the spec remains IN PROGRESS.

Record subsequent phase dates, decisions, exact commands/results, artifact
locations, manual checks, and limitations here. Do not create a separate
completion diary or mark preparation COMPLETE merely because planning passed.

[evidence-spec]: 012-comparison-evidence-and-paper-integration.md
[paper-spec]: 013-comparison-paper.md
[weight-spec]: 010-production-cache-scheduling.md
[workflow]: ../../docs/whitepaper/benchmarks/guides/workflow.md
[adapters]: ../../benchmark/paper/README.md
[development]: ../../docs/whitepaper/benchmarks/guides/DEVELOPMENT.md
[paper-guide]: ../../docs/whitepaper/README.md
[cpp-style]: ../../docs/style-guides/cpp.md
[contributing]: ../../CONTRIBUTING.md
[runner]: ../../docs/whitepaper/benchmarks/lib/run.py
[modules]: ../../benchmark/paper/modules.hpp
[process-table]: ../../docs/latex/deprecated/whitepaper/data/comparison-012/tables/primary/process-timings.csv
[host-snapshot]: ../../docs/latex/deprecated/whitepaper/data/comparison-012/host/pre-confirm-01.json
[reviews]: https://github.com/Arhythmetic-Units/Fourier/blob/a7650346cfb81d9bb454d4d470a45b5dd5c39d77/docs/whitepaper/reviews/2026-09-30-Opus-5-5-M/meta-review.md
[reliability-chat]: codex://threads/01a0f32a-4bf7-7fd0-bfc2-c641595e07d8

### Phase 4 Implementation: September 30, 2026

Added explicit shipped-default module identities and effective float panel-value
records. Independent replay checks all active and inactive lanes, input voltage
sums, AC/DC paths, non-unit gains, spectra, octave/temporal smoothing, Fourier
coordinates and publication cadence. Controlled Fourier N=16384/H=240 and both
actual defaults pass. Existing module timings now carry a separate versioned
module numerical policy; historical artifacts retain their old coverage.

The [engine guide](../../docs/whitepaper/benchmarks/guides/engine.md) defines the
actual pinned Rack `stepBlock` boundary, engine wrappers, native complete-module
controls, fixed background load, consumer cadence/stalls, allocation replay and
lifecycle behavior. All engine cases use preallocated observation storage and a
single mailbox consumer. Held-snapshot tests cover publication and serialized
reset while another thread owns the previous spectrum. Replay checks every
published bin and timestamp; consumer hashes check held-buffer stability without
assuming two FFTW plans reproduce identical rounding.

Source inspection of Rack `8c33d966d329e4a6e354593b2b5f9ac2df5a03bd` confirms
its per-sample worker barriers, inherited worker scheduling, block mutex/shared
lifecycle lock, exclusive reset/rate lock and FPU reset. The runtime sidecar
observes the caller's actual policy; worker FPU/scheduling is source-derived.
Empty-engine controls, 1/4 workers, 1/4/16 analyzers, D=64/256, alignment and fixed
0/16/64-filter loads are supported. Engine CPU accounting includes the measured
loop's workers and consumer; full block observations support budget misses and
frame-intersection peaks. Consumption ages are bounded using block progress;
final drain is labeled separately. Lifecycle passes are separate and serial:
lock acquisition is included, but UI-induced contention is not isolated.

The lifecycle oracle initially advanced Fourier's DC history while frozen.
Inspection showed conditioning also pauses, while Fourier's analysis continues.
Correcting that independent model made freeze/resume pass without changing the
module's DSP behavior or tolerances. Both native controls preserve display
history on sample-rate change and keep mailbox storage alive through reset.

Validation run (all timing fixtures use synthetic clocks):

```shell
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests -p test_workload_controls.py
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests -p test_engine_host.py
CXX='clang++ -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' python3 -m unittest discover -s docs/whitepaper/benchmarks/tests -p test_engine_host.py
make test-rack
make test-mailbox INSTRUMENT=tsan
make benchmark-paper-build PAPER_VDSP=1 PAPER_FFTW_PREFIX=.build/deps/fftw
make -j2
git diff --check
```

The Python suite passed 113 tests with the existing optional plotting skip;
module fixtures passed five tests; engine fixtures passed four normally and
under ASan/UBSan. The headless Rack suites and mailbox TSan check passed. The
plugin and both-provider benchmark builds succeeded. Existing SDK deprecation
warnings and the local FFTW deployment-target warning remain. No Rack GUI,
audio device, performance pass, calibration campaign or user pilot was run.

Scope limitations are explicit in the guide: benchmark-only timestamp stores
add cost; native module shells retain unused production storage; lifecycle
allocation replay counts C++ allocations only; headless consumers hash values
rather than render. Source-independent all-output replay and deterministic
ownership checks satisfy this preparation gate, not a claim of measured speedup
or complete real-time safety. Phase 6 supplies the focused collection matrix;
Phase 7 remains responsible for the protected offline launch package.

### Phase 5 Implementation: September 30, 2026

[Scheduling Experiment Readiness](../../docs/whitepaper/benchmarks/guides/scheduling-experiments.md)
records each candidate's readiness or reasoned deferral. Native hybrid scalar
and true four-channel pipelines now accept fixed completion horizons
`native-horizon-half-v1` and `native-horizon-quarter-v1`. Their H_c values use
ceiling division, preserve 1 <= H_c <= H, and publish at jH+H_c-1. Existing
full-hop and immediate backends complete the comparison. Idle calls continue
capture, frames retain N+H input storage, and temporal smoothing keeps the
original hop cadence. Both C++ and Python contracts, diagnostics, and numerical
coverage recognize the policies. Other backends reject them explicitly.

An in-flight H/H_c mutation is deliberately not exposed. Destruction and
reconstruction cancel the old frame and reset caches; fixtures exercise that
boundary with changed H/H_c while the old cache is incomplete. Tests also cover
H=1, H>N, non-divisible H, wraparound, all supported v3 windows, zero-work idle
calls, stage crossings and every lane/bin. The underlying production DSP and
module schedules retain their existing behavior.

Native leaves, new stage weights, and separate ring/butterfly/magnitude/coordinate
kernel variants are explicitly deferred, with their required follow-up contracts
and limits on paper claims documented. Efficient native and horizon comparisons
are ready first; there is no evidence yet to justify tuning those additional
implementations. The guide discloses spec 010's earlier M1 Pro tuning and gives
predeclared tuning/held-out dimensions for a later weights/leaves study. This
satisfies the phase's permitted readiness-or-deferral gate, not an assertion
that every candidate was implemented. The weighted-operation counterexample
remains explicit: credit quotas alone imply no wall-time or WCET bound.

Validation run:

```shell
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests
CXX='clang++ -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer' python3 -m unittest discover -s docs/whitepaper/benchmarks/tests -p test_native.py
make test/dsp/test_spectrum_analysis
make benchmark-paper-build PAPER_VDSP=1 PAPER_FFTW_PREFIX=.build/deps/fftw
git diff --check
```

The Python suite passed 114 tests with one optional plotting skip. All five
native fixture tests passed normally and under ASan/UBSan, including horizon
coverage and unchanged existing synthesis/analysis paths. The DSP suite passed
2,180,346 assertions in 13 cases. Both-provider executables built successfully.
All recording tests used synthetic clocks; no timings or winners were collected.

### Phase 6 Preparation Blocker: September 30, 2026

The autonomous implementation stopped at a real numerical gate failure. This
spec remains **IN PROGRESS** and must not be archived or handed off as a ready
measurement package. Phases 6 and 7 are incomplete. The extended decay check
also reopens Phase 4's stress acceptance item; its earlier bounded checks still
passed, but did not reach the failing amplitude range.

The draft [pilot design](../../docs/whitepaper/benchmarks/profiles/study-014.json)
and pure `study_plan.py` resolver retain 194 cells / 388 fresh processes per
session across seven groups, two explicit order seeds, candidate practical
thresholds, fixed stopping/retention rules and descriptive quantile ranks.
Five long module-decay cases remain in the plan rather than being omitted to
make it pass. The design is explicitly `readiness: blocked`. These are draft
planning inputs, not a prepared manifest, confirmation freeze or launch promise.
Confirmation-seed enforcement, immutable artifact preparation and the offline
launch package have not been implemented in this phase.

The new [long-decay reproducer profile](../../docs/whitepaper/benchmarks/profiles/engine/long-decay-regression.json)
passes finite input for 4096 samples and then silence, with actual Rack FPU
reset and all-output replay. It is an untimed correctness check. Under the
existing `spectrum-norms-v1` limit of 0.0003:

| Complete Path | First Failing Publication Frame | Relative L2 | Relative Linf |
| --- | --- | --- | --- |
| Fourier default | 106560 | 0.01066863 | 0.01652446 |
| Fourier native vDSP hybrid | 109440 | 0.01066863 | 0.01652446 |
| Fourier native FFTW hybrid | 106560 | 0.01066863 | 0.01652446 |
| Spectre default | 194560 | 0.00032711 | 0.00017642 |
| Spectre native PFFFT hybrid | 109568 | 0.01066863 | 0.01652446 |

These are sample coordinates and numerical errors, not benchmark durations.
The complete stdout/stderr failure summaries and binary/Rack/source identities
are retained in [failure-evidence.json](../../docs/whitepaper/benchmarks/tests/fixtures/decay/failure-evidence.json).
No numerical tolerance was widened and no failing observation was excluded.

The isolated [magnitude reproducer](../../test/paper/reproduce_tiny_magnitude.cpp)
confirms one cause: Rack's SIMD complex `abs` calls its `hypot`, implemented as
`sqrt(a*a+b*b)`. Under ARM64 FPU control 16777216 (flush-to-zero), finite normal
inputs `(1e-25,1e-25)` produce zero, while a binary64 reference gives
`1.414213590008923e-25`, also a normal binary32 magnitude. Intermediate squaring
underflows. PFFFT/FFTW benchmark magnitude adapters also use direct squares.
The exact contribution of underflow, transform rounding and the near-zero
reference policy to every complete path, especially scalar Spectre, is not yet
isolated. This is not evidence of an audible defect or ordinary-amplitude error.

From the repository root, reproduce the failing correctness gate with the
both-provider build from Phase 5:

```shell
DYLD_LIBRARY_PATH=../.. .build/benchmark/rack/paper --engine-verify docs/whitepaper/benchmarks/profiles/engine/long-decay-regression.json
c++ -std=c++11 -O3 -funsafe-math-optimizations -I../../include -I../../dep/include test/paper/reproduce_tiny_magnitude.cpp -L../.. -lRack -o /tmp/fourier-tiny-magnitude
DYLD_LIBRARY_PATH=../.. /tmp/fourier-tiny-magnitude
python3 -m unittest discover -s docs/whitepaper/benchmarks/tests -p test_study_plan.py
```

Both reproducer commands deliberately exit 1 on the observed failure. The two
pure planning tests pass. The Phase 5 full suite remains 114 passing tests and
one optional plotting skip; it did not include this newly extended decay gate.
No performance campaign was launched.

Before continuing, establish a documented tiny-signal/FTZ contract and either
fix the applicable magnitude calculations under a distinct, validated behavior
change or retain their underflow as an explicit numerical limitation with a
separate stress reporting contract. Investigate scalar Spectre independently.
Preserve the old/new identities if arithmetic changes. Do not silently loosen
the relative norm gate, shorten the decay, disable Rack's FPU policy, or drop
these cells. Then rerun the complete decay oracles, finish Phase 6's seed and
integrity policies, and implement Phase 7's user-owned offline launch package.


### Decay Blocker Resolution: September 30, 2026

The user authorized investigating and removing the blocker. The core and native
analysis adapters now calculate scaled complex magnitudes without squaring the
input scale. The isolated reproducer retains the old Rack result of zero and
checks the repaired value against binary64 hypot. SIMD/scalar tiny and large
normal lanes, every native provider's tiny impulse spectrum, and all five full
million-sample module decay paths have regression coverage.

The original premature underflow was fixed. A distinct later limit remains:
Rack flushes binary32 subnormals, and Fourier's default 4.5 dB/octave display
slope magnifies the floor when its ordinate is converted back to FFT units.
The explicit `module-decay-ftz-v1` stress contract and its derived display/base
absolute floors are documented in the engine guide. Above the flagged region,
the original relative tolerance is unchanged; below it every output, absolute
error, relative diagnostic and coverage count is retained. These tails are not
relative-accuracy successes. Invalid/nonfinite or excessive absolute errors
still fail, and ordinary workloads keep `spectrum-norms-v1`. This engineering
floor is not a formal roundoff bound. Historical failures remain unchanged.

Validation: all five engine fixtures, including the five extended decay replays,
and all five native fixture tests passed. The spectrum DSP suite passed
2,180,346 assertions in 13 cases; Rack spectrum-point checks include 40 new
scalar/SIMD magnitude assertions. Final full-suite/build checks are recorded
with the next validation result below. No performance measurement was run.

Final repair validation: `python3 -m unittest discover -s
 docs/whitepaper/benchmarks/tests` passed 117 tests with one optional plotting
skip. `make test-spectrum-points test/dsp/test_spectrum_analysis
benchmark-paper-build PAPER_VDSP=1 PAPER_FFTW_PREFIX=.build/deps/fftw` passed:
Rack spectrum-point tests reported 5,085,458 assertions in 10 cases, DSP reported
2,180,346 assertions in 13 cases, and both benchmark executables built. The
existing SDK and FFTW deployment-target warnings remain. `git diff --check`
passed. This closes Phase 4's reopened stress gate; Phase 6/7 preparation follows.


### Phase 6 Implementation: September 30, 2026

The versioned pilot profile and resolver now declare 194 cells / 388 fresh
processes per session, two distinct order seeds, all seven groups, fixed stopping
and retention rules, practical design thresholds, and observation-rank warnings.
The resolved plan ID is
`55c4bfa25b65f0923f9cb039b3e49da8a5365c4d5521751a69688cee02aad983`.
Its readiness is `implementation-ready`; Phase 7 must bind it to actual prepared
artifacts before launch. The two intended entry commands remain
`make benchmark-study-run SESSION=pilot-01` and `SESSION=pilot-02`.

`prepared.py` seals source membership, source/dependency and artifact bytes,
resolved plan and host/workspace identity. Mutation, missing/additional files,
changed host, malformed/synthetic manifests and stale inputs fail explicitly.
Phase 7 connects these tested checks to the existing serial runner. Job order
is fixed independently of observations; incomplete cells remain failures, never
implicitly omitted or retried. Existing raw summaries retain process identities
and slow observations; design summaries additionally record effective hops,
simulated seconds, quantile ranks and inadequate-tail flags.

Freeze validation supports `predeclared-session-seeds-v1` while keeping old
fixed-seed manifests strict. The new policy requires two pilot records, at least
three declared confirmation sessions and five processes per cell; confirmation
selection requires three separate calendar days. Synthetic fixtures require an
explicit test-only freeze marker, cannot launch a measurement freeze, and fail
real confirmation/publication evidence gates. No real freeze was generated.
The retirement-mechanics test explicitly mocks that evidence boundary; separate
failure tests exercise the unmocked synthetic rejection.

Validation passed: four study planning/policy tests, eight workflow tests and
seven scheduling-metric tests, all using synthetic records. `git diff --check`
passed. Planning arithmetic is 959.147 nominal paced audio seconds plus 388
seconds of process settling and 180 seconds of session settling; this excludes
planning, warmup, replay, I/O, overruns and teardown and is not a measured runtime.
No pilot was run. The offline launch implementation is the next phase.


### Phase 7 Implementation And Pending Final Preparation: September 30, 2026

`benchmark-study-prepare`, `benchmark-study-check` and `benchmark-study-run`
now dispatch through the existing benchmark workflow. `run.py` shares its serial
measurement loop between ordinary built campaigns and immutable prepared stream
or engine groups. Launch performs no builds. Every group is staged before one
180-second gate; each process retains the existing one-second settle policy.
The launcher applies caffeinate, checks AC/power/assertions, records an explicit
quiet-host declaration, enforces a host-wide lock, preserves failures and raw
observations, and packages an offline handback with restricted binaries omitted.

The new `--resources-untimed` mode collects allocation/storage information with
null times and no measurement-clock reads. A C++ synthetic-clock test enforces
that distinction. Numerical preflight and exact engine-profile replay remain
untimed. The launcher does not run reports, select winners, update the paper or
schedule follow-ups. Commands, retained limitations and interruption behavior
are documented in `guides/offline-study.md`.

Seven offline launch fixtures passed, covering shared stream/engine dispatch,
fixed repetitions, gate order, offline sockets, declaration, lock contention,
stale-input rejection, existing-output rejection, failed/interrupted children,
complete integrity and final archive omissions/checksums. Fourteen existing
execution fixtures also passed, including caffeinate assertion lifetime and
power gates. The plugin build succeeded. The full suite and actual network-denied
preparation/check must still pass before this spec is marked COMPLETE and moved.
A network-denied sandbox was verified: its attempted loopback connection failed
with `PermissionError`, independently of the Python fixture socket guard.


The final full Python suite passed 126 tests with one optional plotting skip.
The standalone Make dispatch dry run confirmed launch/check have no compilation
prerequisites. The offline shared-runner fixtures passed after the final
publication-coverage metadata adjustment. Implementation is committed before
preparation so the archived source revision identifies the tested code; final
spec completion will record the prepared manifest and network-denied checks.


### Phase 7 Validation And Handoff: September 30, 2026

All preparation phases are complete. The code package passed actual preparation
and launch-fixture checks under macOS sandbox rules denying every network
operation. The sandbox's denial was independently verified with a rejected
loopback connection. No connected agent service is part of the commands.

```shell
/usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' make benchmark-study-prepare
/usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' make benchmark-study-check
```

The first verified package used source commit `ec87cab` with a clean working
tree and manifest
`f7e002e0c3bff9facc2b7c65ff4026ffcb9be17143362d10f7bece1f0ef32b75`.
It passed 97 distinct stream contract/resource checks and all 94 exact engine
profiles with independent all-output replay and separate lifecycle allocation
checks. The five long-decay profiles reached zero-reference tails: Fourier
checked 2976 vectors per path (2411 zero-reference, 2554 flagged tail), and
Spectre checked 1040 per path (841 zero-reference, 868 flagged tail). These
counts include pre-roll and warmup; they are correctness coverage, not timing
observations. The offline check passed seven launch fixtures, fourteen execution
fixtures and two immutable-study policy fixtures, then verified the manifest.

The package is preserved as `.build/study-014/verified-before-archive` while
archive links are updated. The fresh final package at `.build/study-014/prepared`
captures those documentation bytes; its manifest identity and final check
are recorded below. This refresh runs the same preparation-only commands.

The absolute repository is
`/Users/christiankauten/Documents/Projects/Rack/plugins/Fourier`.
After quiet-host preparation and closing agents, use a standalone terminal:

```shell
cd /Users/christiankauten/Documents/Projects/Rack/plugins/Fourier
make benchmark-study-run SESSION=pilot-01
```

Run `make benchmark-study-run SESSION=pilot-02` only in a separately prepared
host session. Each command collects 194 cells / 388 fresh processes under its
predeclared seed, automatically applies caffeinate and the stabilization gates,
and asks for the explicit `READY` isolation declaration. Follow the
[offline guide](../../docs/whitepaper/benchmarks/guides/offline-study.md) for full
power/isolation, failure, interruption and artifact-retention instructions.
Default outputs and handback files are under
`/Users/christiankauten/Fourier-benchmarks/spec014/`, named `pilot-01/`,
`pilot-01.handback.tar.gz`, `pilot-01.handback.tar.gz.sha256` (and corresponding
`pilot-02` names). Return both session archives and checksum files. A successful
session has `COMPLETE` and `session.json`; failures retain `FAILED` or
`INTERRUPTED` and every attempted raw/log file. Ctrl-C requests cleanup and
packaging. Forced termination cannot guarantee a finished archive.

No measured pilot, real pacing/caffeinate session, thermal characterization,
audio-device experiment or Rack GUI session was performed. The plugin build,
DSP/Rack numerical checks, synthetic recording tests and offline preparation
are distinct validations. Provider opacity and the existing FFTW deployment
warnings remain; compatibility with older macOS releases is unverified. Native
leaves, new weights and separate performance-kernel candidates remain explicitly
deferred. Paper figures, conclusions, confirmation selection and publication
are subsequent work after the user returns real observations.


### Final Immutable Package: September 30, 2026

The refreshed preparation and `benchmark-study-check` both passed with OS-level
network access denied. The final package is:

```text
/Users/christiankauten/Documents/Projects/Rack/plugins/Fourier/.build/study-014/prepared
```

Its manifest ID is
`a53795c22f18c1aa20fb1cf54e08a3f8a14a3fef896aafdd90ce6f7bf29d34a4`.
The archived build source revision is
`b2641cc46ffe57d938c54567f367a00df1f30e3b`, with a clean working tree at preparation.
The package records Apple M1 Pro, arm64, macOS 26.6.2 and Python 3.14.2.
The final primary executable SHA256 is
`f73bac410ca7ac0fe3c32712149a8c8595d9d7f8bbf7318646535986e4bc6d0d`;
the allocation executable SHA256 is
`f9c09297d80f008eb68edfd2ce10d1cafd6394364bf5e28a04687c08f9aa6942`.

All 97 distinct stream checks and 94 exact engine/lifecycle replays passed again.
The final offline check passed its seven launch fixtures, fourteen execution
fixtures and two study-policy fixtures and authenticated the final manifest.
The full suite ran 126 tests (125 passed, one optional plotting skip). Repository
links and `git diff --check` passed. This final evidence-only spec update does
not change any prepared source, dependency, profile, executable or launch byte.
The prepared manifest was revalidated after the update.

The two user-run commands and output/archive paths above are now ready. No
performance observations were collected during either preparation. This spec
is COMPLETE and archived; only the explicitly deferred experiments and later
user measurement/analysis work remain outside its completed scope.
