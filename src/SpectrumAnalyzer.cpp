// A spectrum analyzer module.
//
// Copyright 2025 Arhythmetic Units
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
//

#include <algorithm>  // std::max
#include <array>
#include <vector>
#include <string>     // std::string
#include <limits>     // std::numeric_limits
#include <iomanip>    // std::fixed, std::setprecision
#include "./plugin.hpp"
#include "rack_extensions/panel.hpp"
#include "dsp/constants.hpp"
#include "dsp/dc_blocker.hpp"
#include "dsp/eurorack.hpp"
#include "dsp/spectrum_analysis.hpp"
#include "rack_extensions/display_mailbox.hpp"
#include "rack_extensions/spectrum_coordinates.hpp"
#include "dsp/math.hpp"
#include "dsp/threshold_trigger.hpp"
#include "dsp/trigger_divider.hpp"
#include "dsp/western_scale.hpp"
#include "dsp/window.hpp"

/// @brief A spectrum analyzer module.
struct SpectrumAnalyzer : Module {
 public:
    /// Architectural constants of the module.
    enum Architecture {
        /// The number of processing lanes on the module
        NUM_CHANNELS = 4,
        MAX_FFT = 16384
    };

    /// Controllable parameters on the module.
    enum ParamIds {
        ENUMS(PARAM_INPUT_GAIN, NUM_CHANNELS),
        PARAM_RUN,
        PARAM_WINDOW_FUNCTION,
        PARAM_WINDOW_LENGTH,
        PARAM_HOP_LENGTH,
        PARAM_FREQUENCY_SCALE,
        PARAM_MAGNITUDE_SCALE,
        PARAM_TIME_SMOOTHING,
        PARAM_FREQUENCY_SMOOTHING,
        PARAM_LOW_FREQUENCY,
        PARAM_HIGH_FREQUENCY,
        PARAM_SLOPE,
        NUM_PARAMS
    };

    /// Input ports on the module.
    enum InputIds {
        ENUMS(INPUT_SIGNAL, NUM_CHANNELS),
        NUM_INPUTS
    };

    /// Output ports on the module.
    enum OutputIds {
        NUM_OUTPUTS
    };

    /// LED lights on the module.
    enum LightIds {
        LIGHT_RUN,  // Running indicator
        NUM_LIGHTS
    };

    /// Complete curve snapshot. All storage is sized once; count selects live bins.
    struct DisplaySpectrum {
        std::array<std::vector<Vec>, NUM_CHANNELS> points;
        size_t count = 0;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        // Benchmark-only metadata travels with the same owned snapshot.
        uint64_t sequence = 0;
        int64_t endpoint = -1, published_at = -1;
#endif
        DisplaySpectrum() {
            for (auto& lane : points) lane.resize(MAX_FFT / 2 + 1);
        }
    };


 private:
    /// The sample rate of the module.
    float sample_rate = 0.f;

    /// Double feedback state avoids accumulated DC bias from float rounding
    /// in short periodic inputs. Analysis and display storage remain float.
    std::array<Fourier::DCBlocker<double>, NUM_CHANNELS> dc_blockers;

    /// Engine-owned analysis and a frame's latched coordinate settings.
    // Each bin maps four display coordinates on the engine thread. Give this
    // output stage extra credit so cache rebuilds do not compress its bursts.
    Fourier::SpectrumAnalysis<simd::float_4, 2> analysis{MAX_FFT, MAX_FFT};
    Fourier::SpectrumCoordinates coordinates;
    Fourier::CachedSpectrumCoordinates<MAX_FFT / 2 + 1> coordinate_cache;

    /// Producer writes scheduled bins; only the UI consumes complete frames.
    Fourier::DisplayMailbox<DisplaySpectrum> display_spectrum;

    /// A clock divider for updating the lights at a lower sampling rate.
    Fourier::TriggerDivider light_divider;

    /// A trigger for handling presses on the "run" button.
    Fourier::ThresholdTrigger<float> run_trigger;

    /// A flag determining whether the analyzer is running or not.
    bool is_running = true;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
    uint64_t benchmark_sequence = 0;
    int64_t benchmark_now = 0, benchmark_endpoint = -1, benchmark_last_capture = -1;
#endif

 public:
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
    /// @brief Benchmark replay only, after the engine barrier; never a UI read.
    uint64_t benchmark_publications() const { return benchmark_sequence; }
#endif
    /// @brief UI-only: acquire the latest complete spectrum or retain the last one.
    const DisplaySpectrum& consume_display_spectrum() {
        display_spectrum.consume();
        return display_spectrum.current();
    }

    /// Whether to fill the plots.
    bool is_fill_enabled = false;

    /// Whether to use Bezier curves.
    bool is_bezier_enabled = true;

    /// Whether to apply AC coupling to input signal.
    bool is_ac_coupled = true;

    /// @brief Initialize a new spectrum analyzer.
    SpectrumAnalyzer() : sample_rate(APP->engine->getSampleRate()) {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // Setup input signals and gain parameters.
        static constexpr const char* INPUT_NAMES[NUM_CHANNELS] = {"Red", "Green", "Blue", "Yellow"};
        for (std::size_t i = 0; i < NUM_CHANNELS; i++) {
            configParam(PARAM_INPUT_GAIN + i, 0.f, std::pow(10.f, 12.f / 20.f), 1.f, std::string(INPUT_NAMES[i]) + " Gain", "dB", -10, 20);
            configInput(INPUT_SIGNAL + i, INPUT_NAMES[i]);
        }
        // Configure the run button.
        configButton(PARAM_RUN, "Run");
        getParamQuantity(PARAM_RUN)->description =
            "Enables or disables the analyzer. When disabled,\n"
            "the analyzer stops buffering and processing new audio.";
        // Setup the window function as a custom discrete enumeration.
        configSwitch(PARAM_WINDOW_FUNCTION, 0, Fourier::Window::names().size() - 1, static_cast<size_t>(Fourier::Window::Function::Flattop), "Window", Fourier::Window::names());
        getParamQuantity(PARAM_WINDOW_FUNCTION)->description =
            "The window function to apply before the FFT. Windowing\n"
            "helps reduce spectral leakage in the frequency domain.";
        // Setup the window length as powers of 2 from 2^7=128 to 2^14=16384
        configParam(PARAM_WINDOW_LENGTH, 7.f, 14.f, 11.f, "Length", "", 2, 1);
        getParamQuantity(PARAM_WINDOW_LENGTH)->snapEnabled = true;
        getParamQuantity(PARAM_WINDOW_LENGTH)->description =
            "The FFT size as a power of two. Larger sizes provide\n"
            "higher spectral resolution but require more computation.";
        // Setup hop length in seconds with millisecond render scaling.
        configParam(PARAM_HOP_LENGTH, 0.005, 0.300, 0.030, "Hop", "ms", 0, 1000);
        getParamQuantity(PARAM_HOP_LENGTH)->displayPrecision = 3;
        getParamQuantity(PARAM_HOP_LENGTH)->description =
            "The hop size for the time-domain segmentation (STFT.)\n"
            "The analyzer computes a new FFT along this period.";
        // Setup the discrete frequency scale selector.
        configSwitch(PARAM_FREQUENCY_SCALE, 0, frequency_scale_names().size() - 1, static_cast<size_t>(FrequencyScale::Logarithmic), "Freq Scale", frequency_scale_names());
        getParamQuantity(PARAM_FREQUENCY_SCALE)->description =
            "The frequency-axis scale on the display. The DFT spaces\n"
            "frequencies linearly but humans hear frequencies along\n"
            "a logarithmic scale.";
        // Setup the discrete magnitude scale selector.
        configSwitch(PARAM_MAGNITUDE_SCALE, 0, magnitude_scale_names().size() - 1, static_cast<size_t>(MagnitudeScale::Logarithmic60dB), "Y Scale", magnitude_scale_names());
        getParamQuantity(PARAM_MAGNITUDE_SCALE)->description =
            "The magnitude scale on the display. The DFT spaces\n"
            "magnitude linearly but humans hear volume along\n"
            "logarithmic scales.";
            // Linear, Logarithmic (60dB range), or Logarithmic (120dB range).";
        // Setup time smoothing in seconds with millisecond render scaling.
        configParam(PARAM_TIME_SMOOTHING, 0.f, 2.5f, 0.f, "Average", "ms", 0, 1000);
        getParamQuantity(PARAM_TIME_SMOOTHING)->displayPrecision = 4;
        getParamQuantity(PARAM_TIME_SMOOTHING)->description =
            "The temporal smoothing filter of the STFT. Higher values\n"
            "increase the averaging duration, making the spectrum move\n"
            "more slowly to provide a general impression of signal\n"
            "frequency content.";
        // Setup frequency smoothing as a custom discrete enumeration.
        configSwitch(PARAM_FREQUENCY_SMOOTHING, 0, frequency_smoothing_names().size() - 1, static_cast<size_t>(FrequencySmoothing::None), "Smooth", frequency_smoothing_names());
        getParamQuantity(PARAM_FREQUENCY_SMOOTHING)->description =
            "The fractional-octave smoothing filter of the DFT. For\n"
            "example, 1/6-oct smoothing reduces fine details in the\n"
            "high frequencies.";
        // Setup the low frequency range selector based on the Nyquist rate.
        configParam(PARAM_LOW_FREQUENCY, 0, sample_rate / 2.f, 0, "LO Freq", "Hz");
        getParamQuantity(PARAM_LOW_FREQUENCY)->description =
            "The lower frequency bound for display. Frequencies below\n"
            "this bound are not shown.";
        // Setup the high frequency range selector based on the Nyquist rate.
        configParam(PARAM_HIGH_FREQUENCY, 0, sample_rate / 2.f, sample_rate / 2.f, "HI Freq", "Hz");
        getParamQuantity(PARAM_HIGH_FREQUENCY)->description =
            "The upper frequency bound for display. Frequencies above\n"
            "this bound are not shown.";
        // Setup the slope along a simple range of values. Use a default value
        // of 4.5dB/oct that will be familiar to SPAN users.
        configParam(PARAM_SLOPE, -9, 9, 4.5, "Slope", "dB/oct");
        getParamQuantity(PARAM_SLOPE)->description =
            "The spectrum's slope around 1kHz. Useful for visually\n"
            "compensating the natural roll-off of high frequency energy\n"
            "in musical signals. Typical values are 4.5 and 3.0.";
        // Disable randomization for all parameters.
        for (size_t i = 0; i < NUM_PARAMS; i++)
            getParamQuantity(i)->randomizeEnabled = false;
        // Module state initialization.
        onReset();
    }

    /// @brief Respond to the module being reset by the host environment.
    inline void onReset() final {
        Module::onReset();
        // Resume analysis and publish an empty display snapshot.
        is_running = true;
        display_spectrum.writable().count = 0;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        display_spectrum.writable().sequence = ++benchmark_sequence;
        display_spectrum.writable().endpoint = -1;
        display_spectrum.writable().published_at = benchmark_now;
#endif
        display_spectrum.publish();
        // Reset hidden menu options.
        is_fill_enabled = false;
        is_bezier_enabled = true;
        is_ac_coupled = true;
        // Act as if the sample rate has changed to reset remaining state.
        onSampleRateChange();
    }

    /// @brief Respond to a change in sample rate from the engine.
    inline void onSampleRateChange() final {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        benchmark_last_capture = benchmark_endpoint = -1;
#endif
        Module::onSampleRateChange();
        sample_rate = APP->engine->getSampleRate();
        analysis.reserve_hop(static_cast<size_t>(std::ceil(sample_rate * 0.3f)));
        analysis.reset();
        // Update lights every 512 engine samples and reset the divider.
        light_divider.setDivision(512);
        light_divider.reset();
        // Update the low frequency bound and preserve settings.
        const auto low_frequency = get_low_frequency();
        getParamQuantity(PARAM_LOW_FREQUENCY)->maxValue = sample_rate / 2.f;
        set_low_frequency(low_frequency);
        // Update the high frequency bound and preserve settings.
        const auto high_frequency = get_high_frequency();
        auto param_high_frequency = getParamQuantity(PARAM_HIGH_FREQUENCY);
        param_high_frequency->maxValue = param_high_frequency->defaultValue = sample_rate / 2.f;
        set_high_frequency(high_frequency);
        // Set the transition width of DC-blocking filters for AC-coupled mode.
        for (auto& filter : dc_blockers) {
            filter.setTransitionWidth(10.0, sample_rate);
            filter.reset();
        }
    }

    // -----------------------------------------------------------------------
    // MARK: Serialization
    // -----------------------------------------------------------------------

    /// @brief Convert the module's state to a JSON object.
    /// @returns a pointer to a new json_t object with the module's state.
    inline json_t* dataToJson() final {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "is_running", json_boolean(is_running));
        json_object_set_new(rootJ, "is_fill_enabled", json_boolean(is_fill_enabled));
        json_object_set_new(rootJ, "is_bezier_enabled", json_boolean(is_bezier_enabled));
        json_object_set_new(rootJ, "is_ac_coupled", json_boolean(is_ac_coupled));
        return rootJ;
    }

    /// @brief Load the module's state from a JSON object.
    /// @param rootJ a pointer to a json_t with state data for this module.
    inline void dataFromJson(json_t* rootJ) final {
        json_t* opt = nullptr;
        if ((opt = json_object_get(rootJ, "is_running")))
            is_running = json_boolean_value(opt);
        if ((opt = json_object_get(rootJ, "is_fill_enabled")))
            is_fill_enabled = json_boolean_value(opt);
        if ((opt = json_object_get(rootJ, "is_bezier_enabled")))
            is_bezier_enabled = json_boolean_value(opt);
        if ((opt = json_object_get(rootJ, "is_ac_coupled")))
            is_ac_coupled = json_boolean_value(opt);
    }

    // -----------------------------------------------------------------------
    // MARK: Parameters
    // -----------------------------------------------------------------------

    /// @brief Return the current sample rate of the module.
    /// @returns The sample rate of the module.
    inline float get_sample_rate() const { return sample_rate; }

    // Window Function

    /// @brief Return the window function.
    /// @returns The window function for computing DFT coefficients.
    inline Fourier::Window::Function get_window_function() {
        const auto value = params[PARAM_WINDOW_FUNCTION].getValue();
        return static_cast<Fourier::Window::Function>(value);
    }

    /// @brief Set the window function.
    /// @param value The window function for computing DFT coefficients.
    inline void set_window_function(const Fourier::Window::Function& value) {
        params[PARAM_WINDOW_FUNCTION].setValue(static_cast<float>(value));
    }

    // Window Length

    /// @brief Return the length of the window.
    /// @returns The length of the window measured in samples.
    inline size_t get_window_length() {
        const float value = params[PARAM_WINDOW_LENGTH].getValue();
        const int exponent = std::isfinite(value) ? static_cast<int>(clamp(value, 7.f, 14.f)) : 11;
        return size_t(1) << exponent;
    }

    /// @brief Set the length of the window.
    /// @param value The length of the window measured in samples. Should be
    /// a power of \f$2\f$, e.g., \f$[1, 2, 4, 8, 16, 32, ...]\f$.
    inline void set_window_length(const size_t& value) {
        params[PARAM_WINDOW_LENGTH].setValue(floorf(log2f(value)));
    }

    // Hop Length

    /// @brief Return the hop length of the windowed DFT in samples.
    /// @returns The number of samples to hop between computations of the DFT.
    inline size_t get_hop_length() {
        const float value = params[PARAM_HOP_LENGTH].getValue();
        const float seconds = std::isfinite(value) ? clamp(value, 0.005f, 0.3f) : 0.03f;
        return std::max(size_t(1), static_cast<size_t>(seconds * sample_rate));
    }

    /// @brief Set the hop length of the windowed DFT in samples.
    /// @param value Number of samples to hop between computations of the DFT.
    inline void set_hop_length(const size_t& value) {
        return params[PARAM_HOP_LENGTH].setValue(value / sample_rate);
    }

    // Frequency Scale

    /// @brief Return the frequency scale setting.
    /// @returns The frequency scale for rendering the X axis.
    inline FrequencyScale get_frequency_scale() {
        const auto value = params[PARAM_FREQUENCY_SCALE].getValue();
        return static_cast<FrequencyScale>(value);
    }

    /// @brief Set the frequency scale setting.
    /// @param value The frequency scale for rendering the X axis.
    inline void set_frequency_scale(const FrequencyScale& value) {
        params[PARAM_FREQUENCY_SCALE].setValue(static_cast<float>(value));
    }

    // Magnitude Scale

    /// @brief Set the magnitude scale setting.
    /// @param value The magnitude scale for rendering the Y axis.
    inline MagnitudeScale get_magnitude_scale() {
        const auto value = params[PARAM_MAGNITUDE_SCALE].getValue();
        return static_cast<MagnitudeScale>(value);
    }

    /// @brief Set the magnitude scale setting.
    /// @param value The magnitude scale for rendering the Y axis.
    inline void set_magnitude_scale(const MagnitudeScale& value) {
        params[PARAM_MAGNITUDE_SCALE].setValue(static_cast<float>(value));
    }

    // Time/Magnitude Smoothing

    /// @brief Return the time smoothing setting.
    /// @returns The time smoothing setting (measured in seconds.)
    inline float get_time_smoothing() {
        return params[PARAM_TIME_SMOOTHING].getValue();
    }

    /// @brief Set the time smoothing setting.
    /// @param value The time smoothing setting (measured in seconds.)
    inline void set_time_smoothing(const float& value) {
        params[PARAM_TIME_SMOOTHING].setValue(value);
    }

    /// @brief Compute the alpha parameter of the time smoothing filter.
    /// @returns The alpha parameter of an EMA smoothing filter.
    inline float get_time_smoothing_alpha() {
        // Determine the length of the smoothing filter.
        const float smoothing_time = params[PARAM_TIME_SMOOTHING].getValue();
        // If smoothing time is 0 or lower, alpha is always 0.
        if (smoothing_time <= 0.f) return 0.f;
        // Determine the hop-rate, i.e., the refresh rate of the DFT.
        const float hop_time = params[PARAM_HOP_LENGTH].getValue();
        // Calculate alpha relative to the hop-rate to keep time normalized.
        return expf(-10.f * hop_time / smoothing_time);
    }

    // Frequency/Magnitude Smoothing

    /// @brief Return the frequency smoothing setting.
    /// @returns The frequency smoothing for rendering the coefficients.
    inline FrequencySmoothing get_frequency_smoothing() {
        const auto value = params[PARAM_FREQUENCY_SMOOTHING].getValue();
        return static_cast<FrequencySmoothing>(value);
    }

    /// @brief Set the frequency smoothing setting.
    /// @param value The frequency smoothing for rendering the coefficients.
    inline void set_frequency_smoothing(const FrequencySmoothing& value) {
        params[PARAM_FREQUENCY_SMOOTHING].setValue(static_cast<float>(value));
    }

    // Low Frequency Bound

    /// @brief Return the lowest frequency to render on the display.
    /// @returns The lower display bound in Hz, capped at the Nyquist frequency.
    inline float get_low_frequency() {
        return fmin(params[PARAM_LOW_FREQUENCY].getValue(), sample_rate / 2.f);
    }

    /// @brief Set the lowest frequency to render on the display.
    /// @param value The lowest frequency to render in Hz. If the value is
    /// above the Nyquist frequency, then the value is clipped.
    inline void set_low_frequency(const float& value) {
        params[PARAM_LOW_FREQUENCY].setValue(fmin(value, sample_rate / 2.f));
    }

    // High Frequency Bound

    /// @brief Return the highest frequency to render on the display.
    /// @returns The upper display bound in Hz, capped at the Nyquist frequency.
    inline float get_high_frequency() {
        return fmin(params[PARAM_HIGH_FREQUENCY].getValue(), sample_rate / 2.f);
    }

    /// @brief Set the highest frequency to render on the display.
    /// @param value The highest frequency to render in Hz. If the value is
    /// above the Nyquist frequency, then the value is clipped.
    inline void set_high_frequency(const float& value) {
        params[PARAM_HIGH_FREQUENCY].setValue(fmin(value, sample_rate / 2.f));
    }

    // Magnitude/Frequency Slope

    /// @brief Return the slope of the Bode plot.
    /// @returns The slope of the Bode plot measured in dB/octave.
    inline float get_slope() {
        return params[PARAM_SLOPE].getValue();
    }

    /// @brief Set the slope of the Bode plot.
    /// @param value The slope of the Bode plot measured in dB/octave.
    inline void set_slope(const float& value) {
        params[PARAM_SLOPE].setValue(value);
    }

    // -----------------------------------------------------------------------
    // MARK: Processing
    // -----------------------------------------------------------------------

    /// @brief Process presses to the "run" button.
    /// @details
    /// Processes the run parameter with a trigger and flips the `is_running`
    /// flag when it fires.
    inline void process_run_button() {
        if (run_trigger.process(params[PARAM_RUN].getValue()))
            is_running = !is_running;
    }

    /// @brief Process input signals.
    /// @details
    /// Normalizes each input, applies optional AC coupling and gain, and
    /// returns one sample per lane for DFT computation.
    inline simd::float_4 process_input_signal() {
        if (!is_running) return simd::float_4(0.f);
        // Buffer signals and gains.
        float signals[NUM_CHANNELS] = {0.f, 0.f, 0.f, 0.f};
        float gains[NUM_CHANNELS] = {1.f, 1.f, 1.f, 1.f};
        for (size_t i = 0; i < NUM_CHANNELS; i++) {
            signals[i] = Fourier::Eurorack::fromAC(inputs[INPUT_SIGNAL + i].getVoltageSum());
            gains[i] = params[PARAM_INPUT_GAIN + i].getValue();
            // Keep filter history warm in bypass, as in AC-coupled mode.
            const double filtered = dc_blockers[i].process(signals[i]);
            if (is_ac_coupled) signals[i] = static_cast<float>(filtered);
        }
        simd::float_4 signals_simd(signals[0], signals[1], signals[2], signals[3]);
        simd::float_4 gains_simd(gains[0], gains[1], gains[2], gains[3]);
        // Return the gain-adjusted sample for the analyzer to buffer.
        return gains_simd * signals_simd;
    }

    /// @brief Latch controls and distribute all analysis/curve work over one hop.
    inline void process_coefficients(const simd::float_4& input) {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        if (is_running) benchmark_last_capture = benchmark_now;
#endif
        if (analysis.is_frame_start()) {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
            benchmark_endpoint = benchmark_last_capture;
#endif
            Fourier::SpectrumSettings settings;
            settings.length = get_window_length();
            settings.hop = get_hop_length();
            settings.window = get_window_function();
            settings.sample_rate = sample_rate;
            const auto smoothing = get_frequency_smoothing();
            settings.octave = smoothing == FrequencySmoothing::None ? 0.f : to_float(smoothing);
            settings.alpha = get_time_smoothing_alpha();
            analysis.configure(settings);
            coordinates.bins = analysis.size() / 2 + 1;
            coordinates.sample_rate = sample_rate;
            coordinates.low_frequency = get_low_frequency();
            coordinates.high_frequency = get_high_frequency();
            coordinates.slope = get_slope();
            coordinates.frequency_scale = get_frequency_scale();
            coordinates.magnitude_scale = get_magnitude_scale();
            coordinate_cache.configure(coordinates);
        }
        const bool complete = analysis.process(input, [this](size_t bin, simd::float_4 value) {
            const auto points = coordinate_cache.map(bin, value);
            for (size_t lane = 0; lane < NUM_CHANNELS; ++lane)
                display_spectrum.writable().points[lane][bin] = points[lane];
        }, is_running);
        if (complete) {
            display_spectrum.writable().count = coordinates.bins;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
            display_spectrum.writable().sequence = ++benchmark_sequence;
            display_spectrum.writable().endpoint = benchmark_endpoint;
            display_spectrum.writable().published_at = benchmark_now;
#endif
            display_spectrum.publish();
        }
    }

    /// @brief Set the lights on the panel.
    /// @param args the sample arguments (sample rate, sample time, etc.)
    inline void process_lights(const ProcessArgs& args) {
        if (!light_divider.process()) return;
        const auto light_time = args.sampleTime * light_divider.getDivision();
        lights[LIGHT_RUN].setSmoothBrightness(is_running, light_time);
    }

    /// @brief Process a sample.
    /// @param args the sample arguments (sample rate, sample time, etc.)
    void process(const ProcessArgs& args) final {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        benchmark_now = args.frame;
#endif
        process_run_button();
        process_coefficients(process_input_signal());
        process_lights(args);
    }
};

/// @brief A display widget for rendering frequency coefficients.
struct SpectrumAnalyzerDisplay : TransparentWidget {
 private:
    /// The vertical (top) padding for the plot.
    const size_t pad_top = 20;
    /// The vertical (bottom) padding for the plot.
    const size_t pad_bottom = 50;
    /// The horizontal (left) padding for the plot.
    const size_t pad_left = 35;
    /// The horizontal (right) padding for the plot.
    const size_t pad_right = 15;
    /// The radius of the rounded corners of the screen
    const int corner_radius = 5;
    /// The background color of the screen
    const NVGcolor background_color = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    /// The stroke color for the axis lines
    const NVGcolor axis_stroke_color = {{{0.1f, 0.1f, 0.1f, 1.0f}}};
    /// The width of the lines to render for axes.
    const float axis_stroke_width = 1;
    /// The font color for the axis text.
    const NVGcolor axis_font_color = {{{1.0f, 1.0f, 1.0f, 1.0f}}};
    /// The font size for the axis text.
    const float axis_font_size = 8;
    /// The stroke color for the cross-hair
    const NVGcolor cross_hair_stroke_color = {{{0.2f, 0.2f, 0.2f, 1.0f}}};

    /// The module to render on the display.
    SpectrumAnalyzer* module = nullptr;

    Fourier::CachedDisplay* axes_cache;

    /// the state of the mouse.
    struct {
        /// Whether the mouse is inside the plot rectangle of a live module.
        bool is_hovering = false;
        /// whether a drag is currently active
        bool is_pressed = false;
        /// whether the drag operation is being modified
        bool is_modified = false;
        /// the current position of the mouse pointer during the drag
        Vec position = {0, 0};
    } mouse_state;

    /// @brief Return the normalized position of the mouse.
    Vec get_mouse_position() {
        Vec position = mouse_state.position;
        // calculate the normalized x,y positions in [0, 1]. Account for
        // padding to ensure relative position corresponds to the plot.
        position.x = (position.x - pad_left) / (box.size.x - pad_left - pad_right);
        position.x = Fourier::clip(position.x, 0.f, 1.f);
        // y axis increases downward in pixel space, so invert about 1.
        position.y = 1.f - (position.y - pad_top) / (box.size.y - pad_top - pad_bottom);
        position.y = Fourier::clip(position.y, 0.f, 1.f);
        return position;
    }

    /// @brief Return the minimum frequency to render on the x axis.
    inline float get_low_frequency() {
        if (module == nullptr) return 0;
        return module->get_low_frequency();
    }

    /// @brief Return the maximum frequency to render on the x axis.
    inline float get_high_frequency() {
        if (module == nullptr) return APP->engine->getSampleRate() / 2;
        return module->get_high_frequency();
    }

 public:
    /// @brief Initialize a new spectrum analyzer display widget.
    /// @param module_ the module to render on the display.
    explicit SpectrumAnalyzerDisplay(SpectrumAnalyzer* module_) :
        TransparentWidget(),
        module(module_) {
        axes_cache = new Fourier::CachedDisplay([this](const DrawArgs& args) { draw_axes(args); });
        addChild(axes_cache);
    }

    // -----------------------------------------------------------------------
    // MARK: Interactivity
    // -----------------------------------------------------------------------

    /// @brief Respond to the mouse exiting the widget.
    void onLeave(const LeaveEvent& e) override {
        // Consume the event to prevent it from propagating.
        e.consume(this);
        // Set the hovering state to false.
        mouse_state.is_hovering = false;
    }

    /// @brief Activate cursor readouts only inside the rendered plot rectangle.
    void onHover(const HoverEvent& e) override {
        mouse_state.position = e.pos;
        const Rect plot(Vec(pad_left, pad_top),
            Vec(box.size.x - pad_left - pad_right, box.size.y - pad_top - pad_bottom));
        mouse_state.is_hovering = module && plot.contains(e.pos);
        // Gutters and the control strip must not capture display hover events.
        if (mouse_state.is_hovering) e.consume(this);
    }

    // -----------------------------------------------------------------------
    // MARK: Rendering
    // -----------------------------------------------------------------------

    /// @brief Draw the X ticks with a linear scale.
    /// @param args the arguments for the current draw call
    void draw_x_ticks_linear(const DrawArgs& args) {
        static constexpr float xticks = 10;
        for (float i = 1; i < xticks; i++) {
            // Determine the relative position and re-scale it to the pixel
            // location on-screen. Since we're drawing a static number of
            // points, the position doesn't change relative to the minimum or
            // maximum frequencies (only the label value will change.)
            float position = i / xticks;
            float point_x = rescale(position, 0.f, 1.f, pad_left, box.size.x - pad_right);
            // Render tick marker
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, point_x, pad_top);
            nvgLineTo(args.vg, point_x, box.size.y - pad_bottom);
            nvgStrokeWidth(args.vg, axis_stroke_width);
            nvgStrokeColor(args.vg, axis_stroke_color);
            nvgStroke(args.vg);
            nvgClosePath(args.vg);
            // Render tick label
            float freq = get_low_frequency() + (get_high_frequency() - get_low_frequency()) * position;
            const auto freq_string = Fourier::freq_to_string(freq);
            nvgFontSize(args.vg, axis_font_size);
            nvgFillColor(args.vg, axis_font_color);
            nvgTextAlign(args.vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
            axes_cache->add_label(Vec(point_x, box.size.y - pad_bottom + 10), freq_string, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
        }
    }

    /// @brief Draw the X ticks with an exponential scale.
    /// @param args the arguments for the current draw call
    void draw_x_ticks_logarithmic(const DrawArgs& args) {
        // Iterate over frequencies exponentially (base 10) starting
        // at at least 100Hz up to the maximum frequency (at most the Nyquist
        // frequency) I.e., follow an exponential series like 100, 1000, etc.
        const auto min_exponent = log10(fmax(100.f, get_low_frequency()));
        const auto max_exponent = log10(get_high_frequency());
        const auto frequency_range = get_high_frequency() - get_low_frequency();
        for (float exponent = min_exponent; exponent < max_exponent; exponent++) {
            // Iterate over harmonics of the base frequency, i.e., if
            // we're at base 100Hz, iterate over 200Hz, 300Hz, ...
            const float base_frequency = powf(10.f, exponent);
            for (float offset = 1.f; offset < 10.f; offset++) {
                // Scale base frequency to offset to the n'th harmonic.
                const float frequency = base_frequency * offset;
                if (frequency >= get_high_frequency()) break;
                nvgBeginPath(args.vg);
                // Re-scale the frequency to a pixel location and render.
                const auto position = sqrt((frequency - get_low_frequency()) / frequency_range);
                nvgMoveTo(args.vg, rescale(position, 0.f, 1.f, pad_left, box.size.x - pad_right), pad_top);
                nvgLineTo(args.vg, rescale(position, 0.f, 1.f, pad_left, box.size.x - pad_right), box.size.y - pad_bottom);
                nvgStrokeWidth(args.vg, axis_stroke_width);
                nvgStrokeColor(args.vg, axis_stroke_color);
                nvgStroke(args.vg);
                nvgClosePath(args.vg);
            }
            // Render a label with the base frequency in kHz.
            const auto freq_string = Fourier::freq_to_string(base_frequency);
            nvgFontSize(args.vg, axis_font_size);
            nvgFillColor(args.vg, axis_font_color);
            nvgTextAlign(args.vg, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
            axes_cache->add_label(Vec(rescale(sqrt((base_frequency - get_low_frequency()) / frequency_range), 0.f, 1.f, pad_left, box.size.x - pad_right), box.size.y - pad_bottom + 10), freq_string, NVG_ALIGN_BOTTOM | NVG_ALIGN_CENTER);
        }
    }

    /// @brief Draw the Y ticks with a linear scale.
    /// @param args the arguments for the current draw call
    void draw_y_ticks_linear(const DrawArgs& args) {
        // Iterate over the levels from 25%-400% in steps of 25%.
        for (int level_offset = 0; level_offset < 17; level_offset++) {
            const auto level = 25 * level_offset;
            const auto y_position = rescale(0.25 * level / 100.f, 0.f, 1.f, box.size.y - pad_bottom, pad_top);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, pad_left, y_position);
            nvgLineTo(args.vg, box.size.x - pad_right, y_position);
            nvgStrokeWidth(args.vg, axis_stroke_width);
            nvgStrokeColor(args.vg, axis_stroke_color);
            nvgStroke(args.vg);
            nvgClosePath(args.vg);
            const auto label = std::to_string(level) + "%";
            nvgFontSize(args.vg, axis_font_size);
            nvgFillColor(args.vg, axis_font_color);
            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            axes_cache->add_label(Vec(pad_left - 3, y_position), label, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }
    }

    /// @brief Draw the Y ticks with a logarithmic scale.
    /// @tparam L the data type for the levels.
    /// @param args The arguments for the current draw call.
    /// @param minimum_level The minimum level to render.
    /// @param maximum_level The maximum level to render.
    /// @param levels The individual magnitude levels to render and label.
    template<typename L>
    void draw_y_ticks_logarithmic(
        const DrawArgs& args,
        const float& minimum_level,
        const float& maximum_level,
        L levels
    ) {
        for (auto& level : levels) {
            // Compute the magnitude of the shifted and scaled level (such that
            // 12dB is the maximum level to render on-screen).
            const auto magnitude = (level - maximum_level) / (maximum_level - minimum_level) + 1.f;
            const auto y_position = rescale(magnitude, 0.f, 1.f, box.size.y - pad_bottom, pad_top);
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, pad_left, y_position);
            nvgLineTo(args.vg, box.size.x - pad_right, y_position);
            nvgStrokeWidth(args.vg, axis_stroke_width);
            nvgStrokeColor(args.vg, axis_stroke_color);
            nvgStroke(args.vg);
            nvgClosePath(args.vg);
            const auto label = std::to_string(level) + "dB";
            nvgFontSize(args.vg, axis_font_size);
            nvgFillColor(args.vg, axis_font_color);
            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            axes_cache->add_label(Vec(pad_left - 3, y_position), label, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }
    }

    /// @brief Draw DFT coefficients as a stroke.
    /// @param args the arguments for the current draw call
    /// @param coefficients The coefficients to render
    /// @param stroke_width The width of the stroke.
    /// @param stroke_color The color of the stroke.
    /// @param fill_color The color of the fill.
    void draw_coefficients(
        const DrawArgs& args,
        const Vec* coefficients,
        size_t count,
        const float& stroke_width,
        const NVGcolor& stroke_color={{{0.9f, 0.85f, 0.15f, 0.75f}}},
        const NVGcolor& fill_color={{{0.9f, 0.85f, 0.15f, 0.5f}}}
    ) {
        // Create a rectangle for masking the curve to the visible area.
        const Rect mask = Rect(Vec(pad_left, pad_top), box.size.minus(Vec(pad_left + pad_right, pad_top + pad_bottom)));
        // Create a new path that is masked to the box of the waveform display.
        nvgSave(args.vg);
        nvgBeginPath(args.vg);
        nvgScissor(args.vg, mask.pos.x, mask.pos.y, mask.size.x, mask.size.y);
        // For fill mode, move to a starting point on the bottom left of the
        // mask that is as far out as the stroke of the line (to hide it.)
        nvgMoveTo(args.vg, mask.pos.x - 2 * stroke_width, mask.pos.y + mask.size.y + 2 * stroke_width);
        // Find and render the first visible frequency bin.
        size_t n = 0;
        for (; n < count; n++) {
            Vec point = coefficients[n];
            // Check if we've reached the visible frequency range.
            if (point.x < 0.f) continue;
            // Remap the coefficient point to the window of the mask.
            point.x = rescale(point.x, 0.f, 1.f, mask.pos.x, mask.pos.x + mask.size.x);
            point.y = rescale(point.y, 0.f, 1.f, mask.pos.y + mask.size.y, mask.pos.y);
            // Clip the point to the visible window.
            point.x = Fourier::clip(point.x, mask.pos.x - 2 * stroke_width, mask.pos.x + mask.size.x + 2 * stroke_width);
            point.y = Fourier::clip(point.y, mask.pos.y - 2 * stroke_width, mask.pos.y + mask.size.y + 2 * stroke_width);
            // Draw an invisible line from the bottom point to the starting point.
            nvgLineTo(args.vg, mask.pos.x - 2 * stroke_width, point.y);
            // Exit the loop as we have found the first point.
            break;
        }
        // Render the visible frequency bins.
        if (module->is_bezier_enabled) {
            for (; n < count - 2; n++) {
                // Determine whether this is the last point to render.
                bool last_point = (coefficients[n+1].x >= 1.f) || (n == count - 3);
                // Create a neighborhood of points to render.
                Vec points[4] = {coefficients[n == 0 ? 0 : n-1], coefficients[n-0], coefficients[n+1], coefficients[n+2]};
                for (Vec& point : points) {
                    // Remap the coefficient point to the window of the mask.
                    point.x = rescale(point.x, 0.f, 1.f, mask.pos.x, mask.pos.x + mask.size.x);
                    point.y = rescale(point.y, 0.f, 1.f, mask.pos.y + mask.size.y, mask.pos.y);
                    // Clip the point to the visible window.
                    point.x = Fourier::clip(point.x, mask.pos.x - 2 * stroke_width, mask.pos.x + mask.size.x + 2 * stroke_width);
                    point.y = Fourier::clip(point.y, mask.pos.y - 2 * stroke_width, mask.pos.y + mask.size.y + 2 * stroke_width);
                }
                Vec control[2];
                catmull_rom_to_bezier(points, control);
                // Draw a cubic Bezier from p[i] to p[i+1] using control points
                nvgBezierTo(args.vg, control[0].x, control[0].y, control[1].x, control[1].y, points[2].x, points[2].y);
                if (last_point) {
                    nvgLineTo(args.vg, mask.pos.x + mask.size.x + 2 * stroke_width, points[3].y);
                    break;
                }
            }
        } else {
            for (; n < count; n++) {
                Vec point = coefficients[n];
                // Determine whether this is the last point to render.
                bool last_point = (point.x >= 1.f) || (n == count - 1);
                // Remap the coefficient point to the window of the mask.
                point.x = rescale(point.x, 0.f, 1.f, mask.pos.x, mask.pos.x + mask.size.x);
                point.y = rescale(point.y, 0.f, 1.f, mask.pos.y + mask.size.y, mask.pos.y);
                // Clip the point to the visible window.
                point.x = Fourier::clip(point.x, mask.pos.x - stroke_width, mask.pos.x + mask.size.x + stroke_width);
                point.y = Fourier::clip(point.y, mask.pos.y - stroke_width, mask.pos.y + mask.size.y + stroke_width);
                // Connection to the next point in the plot.
                nvgLineTo(args.vg, point.x, point.y);
                // Connection to stop point for fill.
                if (last_point) {
                    nvgLineTo(args.vg, mask.pos.x + mask.size.x + 2 * stroke_width, point.y);
                    break;
                }
            }
        }
        // For fill mode, move to a stopping point on the bottom right of the
        // mask that is as far out as the stroke of the line (to hide it.)
        nvgLineTo(args.vg, mask.pos.x + mask.size.x + 2 * stroke_width, mask.pos.y + mask.size.y + 2 * stroke_width);
        nvgGlobalCompositeOperation(args.vg, NVG_LIGHTER);
        nvgStrokeWidth(args.vg, stroke_width);
        nvgStrokeColor(args.vg, stroke_color);
        nvgStroke(args.vg);
        if (module->is_fill_enabled) {
            nvgFillColor(args.vg, fill_color);
            nvgFill(args.vg);
        }
        nvgResetScissor(args.vg);
        nvgClosePath(args.vg);
        nvgRestore(args.vg);
    }

    /// @brief Draw the mouse position cross-hair.
    /// @param args the arguments for the current draw call.
    void draw_cross_hair(const DrawArgs& args) {
        const auto mouse_position = get_mouse_position();
        // Render the cross-hair row.
        const float y_position = rescale(mouse_position.y, 0, 1, box.size.y - pad_bottom, pad_top);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, pad_left, y_position);
        nvgLineTo(args.vg, box.size.x - pad_right, y_position);
        nvgStrokeWidth(args.vg, 0.5);
        nvgStrokeColor(args.vg, cross_hair_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // Render the cross-hair column.
        const float x_position = rescale(mouse_position.x, 0, 1, pad_left, box.size.x - pad_right);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x_position, pad_top);
        nvgLineTo(args.vg, x_position, box.size.y - pad_bottom);
        nvgStrokeWidth(args.vg, 0.5);
        nvgStrokeColor(args.vg, cross_hair_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
    }

    /// @brief Return the frequency that the mouse is hovering over.
    /// @param mouse_position The position of the mouse on the screen.
    /// @param scale The frequency scale to render the mouse transform with.
    /// @param low_frequency The low frequency bound of the window.
    /// @param high_frequency The high frequency bound of the window.
    static inline float get_hover_freq(
        const Vec& mouse_position,
        const FrequencyScale& scale,
        const float& low_frequency,
        const float& high_frequency
    ) {
        switch (scale) {
        case FrequencyScale::Linear:
            return low_frequency + (high_frequency - low_frequency) * mouse_position.x;
        case FrequencyScale::Logarithmic:
            return (high_frequency - low_frequency) * Fourier::squared(mouse_position.x) + low_frequency;
        default:
            throw std::runtime_error("Invalid frequency scale " + std::to_string(static_cast<int>(scale)));
        }
    }

    /// @brief Return a string representation of the mouse position.
    /// @param mouse_position The position of the mouse on the screen.
    /// @param scale The magnitude scale to render the mouse transform with.
    static inline std::string mouse_position_to_string(
        const Vec& mouse_position,
        const MagnitudeScale& scale
    ) {
        std::ostringstream stream;
        stream << std::fixed << std::setprecision(2);
        switch (scale) {
        case MagnitudeScale::Linear:
            stream << mouse_position.y * 4 * 100 << "%";
            break;
        case MagnitudeScale::Logarithmic60dB:
            stream << rescale(mouse_position.y, 0, 1, -60, 12) << "dB";
            break;
        case MagnitudeScale::Logarithmic120dB:
            stream << rescale(mouse_position.y, 0, 1, -120, 12) << "dB";
            break;
        default:
            throw std::runtime_error("Invalid magnitude scale " + std::to_string(static_cast<int>(scale)));
        }
        return stream.str();
    }

    /// @brief Draw the cross-hair information as text.
    /// @param args the arguments for the current draw call.
    void draw_cross_hair_text(const DrawArgs& args) {
        const auto mouse_position = get_mouse_position();
        auto font_path = asset::plugin(plugin_instance, "res/Font/Arial/Bold.ttf");
        const std::shared_ptr<Font> font = APP->window->loadFont(font_path);
        nvgFontSize(args.vg, 9);
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, {{{0.f / 255.f, 90.f / 255.f, 11.f / 255.f, 1.f}}});
        nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);
        // Render hovered frequency above the plot in the top left.
        const float hover_freq = get_hover_freq(
            mouse_position,
            module->get_frequency_scale(),
            get_low_frequency(),
            get_high_frequency()
        );
        const auto hover_freq_string = Fourier::freq_to_string(hover_freq);
        nvgText(args.vg, pad_left + 3, pad_top / 2, hover_freq_string.c_str(), NULL);
        // Convert the frequency to a note.
        if (hover_freq > 0) {  // Render note, octave, and tuning (in cents.)
            Fourier::TunedNote note(hover_freq);
            nvgText(args.vg, pad_left + 55, pad_top / 2, note.note_string().c_str(), NULL);
            nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
            nvgText(args.vg, pad_left + 140, pad_top / 2, note.tuning_string().c_str(), NULL);
        }
        ;
        // Render the y position.
        const auto mouse_position_string = mouse_position_to_string(mouse_position, module->get_magnitude_scale());
        nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        nvgText(args.vg, box.size.x - pad_right - 3, pad_top / 2, mouse_position_string.c_str(), NULL);
    }

    /// @brief Invalidate static artwork when its rendering inputs change.
    void prepare_axes_cache() {
        axes_cache->prepare(box.size, {{get_low_frequency(), get_high_frequency(),
            module ? module->get_sample_rate() : APP->engine->getSampleRate(),
            static_cast<float>(module ? module->get_frequency_scale() : FrequencyScale::Logarithmic),
            static_cast<float>(module ? module->get_magnitude_scale() : MagnitudeScale::Logarithmic60dB)}});
    }

    /// @brief Rasterize the static background/grid and cache label layout.
    void draw_axes(const DrawArgs& args) {
        // Draw the background.
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, corner_radius);
        nvgFillColor(args.vg, background_color);
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, axis_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // Draw the frequency (X) axis.
        // - Left border
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, pad_left, pad_top);
        nvgLineTo(args.vg, pad_left, box.size.y - pad_bottom);
        nvgStrokeWidth(args.vg, axis_stroke_width);
        nvgStrokeColor(args.vg, axis_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // - Right border
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, box.size.x - pad_right, pad_top);
        nvgLineTo(args.vg, box.size.x - pad_right, box.size.y - pad_bottom);
        nvgStrokeWidth(args.vg, axis_stroke_width);
        nvgStrokeColor(args.vg, axis_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // - Ticks
        switch (module == nullptr ? FrequencyScale::Logarithmic : module->get_frequency_scale()) {
        case FrequencyScale::Linear:
            draw_x_ticks_linear(args);
            break;
        case FrequencyScale::Logarithmic:
            draw_x_ticks_logarithmic(args);
            break;
        default:
            throw std::runtime_error("Invalid frequency scale " + std::to_string(static_cast<int>(module->get_frequency_scale())));
        }
        // Draw the magnitude (Y) axis.
        switch (module == nullptr ? MagnitudeScale::Logarithmic60dB : module->get_magnitude_scale()) {
        case MagnitudeScale::Linear:
            draw_y_ticks_linear(args);
            break;
        case MagnitudeScale::Logarithmic60dB:
            draw_y_ticks_logarithmic(args, -60.f, 12.f, std::vector<int>{12, 0, -12, -24, -48, -60});
            break;
        case MagnitudeScale::Logarithmic120dB:
            draw_y_ticks_logarithmic(args, -120.f, 12.f, std::vector<int>{12, 0, -12, -24, -48, -60, -96, -120});
            break;
        default:
            throw std::runtime_error("Invalid magnitude scale " + std::to_string(static_cast<int>(module->get_magnitude_scale())));
        }
    }

    /// @brief Draw the screen.
    /// @param args the arguments for the current draw call
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {  // Render as a light/display w/o dimming features
            prepare_axes_cache();
            axes_cache->draw_cached(args);
            const auto font = APP->window->loadFont(asset::plugin(plugin_instance, "res/Font/Arial/Bold.ttf"));
            if (font) axes_cache->draw_labels(args, font->handle, axis_font_size, axis_font_color);
            const auto* spectrum = module ? &module->consume_display_spectrum() : nullptr;
            if (spectrum && spectrum->count > 0) {
                draw_coefficients(args, spectrum->points[0].data(), spectrum->count, 1.5, {{{1.f, 0.f, 0.f, 1.f}}}, {{{1.f, 0.f, 0.f, 0.35f}}});
                draw_coefficients(args, spectrum->points[1].data(), spectrum->count, 1.5, {{{0.f, 1.f, 0.f, 1.f}}}, {{{0.f, 1.f, 0.f, 0.35f}}});
                draw_coefficients(args, spectrum->points[2].data(), spectrum->count, 1.5, {{{0.f, 0.f, 1.f, 1.f}}}, {{{0.f, 0.f, 1.f, 0.35f}}});
                draw_coefficients(args, spectrum->points[3].data(), spectrum->count, 1.5, {{{1.f, 1.f, 0.f, 1.f}}}, {{{1.f, 1.f, 0.f, 0.35f}}});
                // Interactive mouse hovering functionality.
                if (mouse_state.is_hovering) {
                    draw_cross_hair(args);
                    draw_cross_hair_text(args);
                }
            }
            // border
            nvgBeginPath(args.vg);
            nvgRect(args.vg, pad_left, pad_top, box.size.x - pad_left - pad_right, box.size.y - pad_top - pad_bottom);
            nvgStrokeWidth(args.vg, axis_stroke_width);
            nvgStrokeColor(args.vg, axis_stroke_color);
            nvgStroke(args.vg);
            nvgClosePath(args.vg);
        }
        TransparentWidget::drawLayer(args, layer);
    }
};

/// @brief The widget for the spectrum analyzer module.
struct SpectrumAnalyzerWidget : ModuleWidget {
    /// @brief Create a new spectrum analyzer widget.
    /// @param module The back-end module to interact with. Can be a nullptr.
    explicit SpectrumAnalyzerWidget(SpectrumAnalyzer* module = nullptr) : ModuleWidget() {
        setModule(module);
        setPanel(new Fourier::Panel(Fourier::PanelKind::FOURIER));
        // Input signal ports and gain controls.
        for (std::size_t i = 0; i < SpectrumAnalyzer::NUM_CHANNELS; i++) {
            addInput(createInput<ThemedPJ301MPort>(Fourier::PanelLayout::input(i), module, SpectrumAnalyzer::INPUT_SIGNAL + i));
            addParam(createParam<Trimpot>(Fourier::PanelLayout::gain(i), module, SpectrumAnalyzer::PARAM_INPUT_GAIN + i));
        }
        // Buttons.
        addParam(createParamCentered<PB61303>(Fourier::PanelLayout::run(), module, SpectrumAnalyzer::PARAM_RUN));
        addChild(createLightCentered<PB61303Light<WhiteLight>>(Fourier::PanelLayout::run(), module, SpectrumAnalyzer::LIGHT_RUN));
        // Screen.
        SpectrumAnalyzerDisplay* display = new SpectrumAnalyzerDisplay(module);
        display->setPosition(Fourier::PanelLayout::display_position());
        display->setSize(Fourier::PanelLayout::display_size(Fourier::PanelKind::FOURIER));
        addChild(display);
        // Screen controls. Rack screenshots and browser previews have no module.
        // Supply display defaults without constructing an engine/DSP instance.
        // Window function control with custom angles to match discrete range.
        auto window_function_param = createParam<TextKnob>(Vec(50 + 0 * 66, 330), module, SpectrumAnalyzer::PARAM_WINDOW_FUNCTION);
        window_function_param->maxAngle = 2.f * M_PI;
        window_function_param->label.text = "WINDOW";
        if (!module) window_function_param->value.text = "Flattop";
        addParam(window_function_param);
        // Window length control with custom angles to match discrete range.
        auto window_length_param = createParam<TextKnob>(Vec(50 + 1 * 66, 330), module, SpectrumAnalyzer::PARAM_WINDOW_LENGTH);
        window_length_param->maxAngle = 1.2f * M_PI;
        window_length_param->label.text = "LENGTH";
        if (!module) window_length_param->value.text = "2048";
        addParam(window_length_param);
        // Hop length control.
        auto hop_length_param = createParam<TextKnob>(Vec(50 + 2 * 66, 330), module, SpectrumAnalyzer::PARAM_HOP_LENGTH);
        hop_length_param->label.text = "HOP";
        if (!module) hop_length_param->value.text = "30ms";
        addParam(hop_length_param);
        // Frequency scale control with custom angles to match discrete range.
        auto frequency_scale_param = createParam<TextKnob>(Vec(50 + 3 * 66, 330), module, SpectrumAnalyzer::PARAM_FREQUENCY_SCALE);
        frequency_scale_param->maxAngle = 0.3 * M_PI;
        frequency_scale_param->label.text = "FREQ SCALE";
        frequency_scale_param->label.font_size = 8.5f;
        if (!module) frequency_scale_param->value.text = "Logarithmic";
        addParam(frequency_scale_param);
        // Magnitude scale control with custom angles to match discrete range.
        auto magnitude_scale_param = createParam<TextKnob>(Vec(50 + 4 * 66, 330), module, SpectrumAnalyzer::PARAM_MAGNITUDE_SCALE);
        magnitude_scale_param->maxAngle = 0.6 * M_PI;
        magnitude_scale_param->label.text = "Y SCALE";
        if (!module) magnitude_scale_param->value.text = "Log 60dB";
        addParam(magnitude_scale_param);
        // Time smoothing control.
        auto time_smoothing_param = createParam<TextKnob>(Vec(50 + 5 * 66, 330), module, SpectrumAnalyzer::PARAM_TIME_SMOOTHING);
        time_smoothing_param->label.text = "AVERAGE";
        if (!module) time_smoothing_param->value.text = "0ms";
        addParam(time_smoothing_param);
        // Frequency smoothing control with custom angles to match discrete range.
        auto frequency_smoothing_param = createParam<TextKnob>(Vec(50 + 6 * 66, 330), module, SpectrumAnalyzer::PARAM_FREQUENCY_SMOOTHING);
        frequency_smoothing_param->label.text = "SMOOTH";
        frequency_smoothing_param->maxAngle = 2.f * M_PI;
        if (!module) frequency_smoothing_param->value.text = "None";
        addParam(frequency_smoothing_param);
        // Low and High frequency (frequency range) controls.
        auto low_freq_param = createParam<TextKnob>(Vec(50 + 7 * 66, 330), module, SpectrumAnalyzer::PARAM_LOW_FREQUENCY);
        low_freq_param->label.text = "LO FREQ";
        if (!module) low_freq_param->value.text = "0Hz";
        addParam(low_freq_param);
        auto high_freq_param = createParam<TextKnob>(Vec(50 + 8 * 66, 330), module, SpectrumAnalyzer::PARAM_HIGH_FREQUENCY);
        high_freq_param->label.text = "HI FREQ";
        if (!module) high_freq_param->value.text = string::f("%.5gHz", APP->engine->getSampleRate() / 2.f);
        addParam(high_freq_param);
        // Slope (dB/octave @1000Hz) controls.
        auto slope_param = createParam<TextKnob>(Vec(50 + 9 * 66, 330), module, SpectrumAnalyzer::PARAM_SLOPE);
        slope_param->label.text = "SLOPE";
        if (!module) slope_param->value.text = "4.5dB/oct";
        addParam(slope_param);
        // Screws
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, 0)));
        addChild(createWidget<ThemedScrew>(Vec(RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
        addChild(createWidget<ThemedScrew>(Vec(box.size.x - 2 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT - RACK_GRID_WIDTH)));
    }

    /// @brief Append the context menu to the module when right clicked.
    /// @param menu the menu object to add context items for the module to
    void appendContextMenu(Menu* menu) override {
        auto module = getModule<SpectrumAnalyzer>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Render Settings"));
        menu->addChild(createBoolMenuItem("Filled Display", "",
            [=]() { return module->is_fill_enabled; },
            [=](bool value) {
                Fourier::set_module_setting(module, "change filled display",
                    "is_fill_enabled", json_boolean(value));
            }));
        menu->addChild(createBoolMenuItem("Bezier Curve", "",
            [=]() { return module->is_bezier_enabled; },
            [=](bool value) {
                Fourier::set_module_setting(module, "change bezier curve",
                    "is_bezier_enabled", json_boolean(value));
            }));
        menu->addChild(createBoolMenuItem("AC-coupled", "",
            [=]() { return module->is_ac_coupled; },
            [=](bool value) {
                Fourier::set_module_setting(module, "change ac-coupled",
                    "is_ac_coupled", json_boolean(value));
            }));
        ModuleWidget::appendContextMenu(menu);
    }
};

Model* modelSpectrumAnalyzer = createModel<SpectrumAnalyzer, SpectrumAnalyzerWidget>("SpectrumAnalyzer");
