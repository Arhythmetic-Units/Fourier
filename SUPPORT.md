# Fourier Support

Find help with Fourier and Spectre, report a problem, or suggest an improvement.
You do not need a source build or coding experience to ask for help.

## Manuals And Troubleshooting

-   [Fourier manual][fourier-manual]: inputs, spectrum controls, frequency and
    amplitude measurements, presets, and troubleshooting.
-   [Spectre manual][spectre-manual]: spectrogram controls, color and level
    settings, time and frequency resolution, presets, and troubleshooting.
-   [Changelog](CHANGELOG.md): fixes and behavior changes by version.

The PDF links target the latest GitHub release, which can differ from the
version distributed by VCV Library or your development checkout. For a source
build, follow the [manual build guide](CONTRIBUTING.md#user-manuals-and-build-products)
to build manuals from the same checkout.

## Questions And Bug Reports

Search the [existing issues][issues] for a matching question or problem.
If none matches, [open an issue][new-issue]. Use the bug report template for
unexpected behavior; for a usage question, explain what you are trying to
do and where you are stuck.

Include your Rack and plugin versions, operating system and CPU architecture,
sample rate, affected module, and enough steps to reproduce the problem.
Describe relevant FFT length, hop/refresh setting, window, smoothing, gain,
AC coupling, display scales, and input ports/polyphony. For unexpected spectra,
include the input signal's frequency in Hz and amplitude with units. Unknown
details can be marked as unknown.

A minimal patch, screenshot, or relevant Rack log excerpt can help. List any
additional plugins or sample files required by the patch. For performance
problems, include module count, audio buffer size, and CPU. Check patches and
logs for personal paths or other private information before sharing them.

## Feature Requests

Use the feature request template in [new issues][new-issue]. Describe the
musical workflow or measurement task, the behavior you would like, and any
workaround you currently use.

## Building And Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for environment setup, builds, tests,
and pull request guidance. For build or test failures, include the commit,
exact command and working directory, tool versions, and relevant error output
in the bug report.

[fourier-manual]: https://github.com/Arhythmetic-Units/Fourier/releases/latest/download/Fourier.pdf
[spectre-manual]: https://github.com/Arhythmetic-Units/Fourier/releases/latest/download/Spectre.pdf
[issues]: https://github.com/Arhythmetic-Units/Fourier/issues
[new-issue]: https://github.com/Arhythmetic-Units/Fourier/issues/new/choose
