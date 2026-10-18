# Change Log

## Unreleased

### Fixed

-   Show default text-control values in Fourier and Spectre module-browser
    previews and Rack's module screenshot exports, including window, FFT
    length, hop, scales, smoothing, frequency bounds, and slope.

## 2.2.1 (2026-10-01)

### Changed

-   Revise the whitepaper as the seven-page conference-style manuscript
    *Scheduling FFT-Based Spectral Analysis in the Audio Processing Loop*,
    manuscript version 5, introducing the CoopFFT name and focusing the
    scheduling analysis and retained experimental evidence.
-   Make the active paper a self-contained LaTeX source with portable export;
    retain the extended report and historical material in the documentation
    archive. Update both manuals and citation metadata for this release.

### Fixed

-   Install the complete New TX font dependencies in publication CI and
    validate the whitepaper metadata against its canonical source. Release
    builds attach both manuals and the whitepaper PDF.

The plugin's DSP, controls, and saved-patch behavior are unchanged from 2.2.0.

## 2.2.0 (2026-09-30)

### Added

-   Add Spectre Decibels and Linear intensity modes with adjustable Floor
    and Ceil endpoints, a color-range legend, and panel dropdowns for the
    palette and intensity scale. Frozen history can be recolored without
    recording again.
-   Add Spectre's `DecibelInspection` preset and separate Raw and Color
    level readouts in Decibels mode.
-   Expand both user manuals with quick starts, practical examples,
    troubleshooting, and annotated panel drawings.

### Changed

-   Start new Spectre instances in Decibels mode with a -90 to 0 dB range.
    Older patches without an intensity mode and the five original presets
    retain Linear mode and their existing colors. Palette and intensity
    selection now live on the color screen and support Undo and Redo.
-   Distribute spectrum analysis and per-bin output work over one exact
    hop in Fourier and Spectre, including windowing, reconstruction,
    smoothing, curve preparation, and snapshot writing. Refresh cadence
    may be slower than the former early-completion schedule.
-   Preallocate processing storage; FFT length, window, and smoothing changes
    rebuild caches incrementally and latch at frame boundaries. Preserve
    saved controls and magnitude conventions.
-   Publish Fourier curves through synchronized snapshots and remove Spectre's
    duplicate engine history. Reset/sample-rate changes discard partial frames.

### Fixed

-   Use floating-point absolute values for windows and decibel conversion
    across standard libraries, and remove the standalone FFT's dependency
    on the nonstandard `M_PI` macro.
-   Prevent AC-coupled DC offsets caused by accumulated float rounding in
    short repeating signals; both modules now retain double-precision filter
    state.
-   Hide display cursor overlays outside the plot rectangles.

## 2.1.2 (2025-04-14)

-   Wrote manual for Spectre

## 2.1.1 (2025-03-13)

-   Fix `Math::freq_to_string` to cover edge cases along unit bounds
-   Update `SpectrumAnalyzer` and `Spectrogram` to use `Math::freq_to_string`
    in all cases of frequency string serialization
-   Remove `FlagMenuItem` in favor of built-in `createBoolPtrMenuItem`
-   Update `Spectrogram` to use `createIndexPtrSubmenuItem` instead of custom
    defined menu options for window function selection
-   Replace `configParam<TriggerParamQuantity>(...)` with `configButton(...)`
-   Replace `NumOptions` with name vector `.size()` call, which is more direct
    and readable
-   Simplify JSON code for the plugin
-   Refactor font use following API guidelines (in functions not instances)
-   Update "screen" padding to prevent graphical glitches and improve over-all
    look of _Fourier_ and _Spectre_ in the module view within VCV Rack

## 2.1.0 (2025-03-10)

-   Support for built-in VCV dark light/dark panels
    -   Deprecates internal management of theme in favor of built-in features
    -   Dark panels may be selected using `View > Use dark panels if available`

## 2.0.2 (2025-03-10)

-   Update manual for _Fourier_ and _Spectre_
-   Resolve error from macOS build chain (`stream = {}` -> refactored functions)
-   Resolve error from Windows build chain (`static constexpr` in displays
    refactored to instance level `const`.)
-   Resolve runtime error from Windows machines on initialization of _Fourier_
    -   Update `SpectrogramAnalyzer::draw_coefficients` to pass coefficient
        buffers by _value_ as opposed to by _reference_ to prevent race
        conditions
-   Resolve runtime error from Windows machines on initialization of _Spectre_
    -   Update `Spectrogram::draw_coefficients` to render pixels on the _heap_
        instead of the _stack_. I.e. replace `uint8_t pixels[size]` with
        `std::vector<uint8_t> pixels; pixels.resize(size);`.

## 2.0.1 (2025-03-08)

-   Resolve compiler error on Windows machines

## 2.0.0 (2025-03-07)

-   Initial release
