// A spectrogram module.
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

#include <array>
#include <complex>
#include <cstdint>
#include <memory>
#include <algorithm>  // std::fill
#include <string>     // std::string
#include <limits>     // std::numeric_limits
#include <iomanip>    // std::fixed, std::setprecision
#include "./plugin.hpp"
#include "rack_extensions/panel.hpp"
#include "rack_extensions/spectrogram_intensity.hpp"
#include "rack_extensions/display_mailbox.hpp"
#include "dsp/color_map.hpp"
#include "dsp/constants.hpp"
#include "dsp/dc_blocker.hpp"
#include "dsp/eurorack.hpp"
#include "dsp/spectrum_analysis.hpp"
#include "dsp/math.hpp"
#include "dsp/threshold_trigger.hpp"
#include "dsp/trigger_divider.hpp"
#include "dsp/western_scale.hpp"
#include "dsp/window.hpp"

/// @brief A spectrogram module.
struct Spectrogram : Module {
 public:
    enum {
        N_FFT = 2048,
        N_STFT = 512
    };

    enum ParamIds {
        PARAM_INPUT_GAIN,
        PARAM_RUN,
        PARAM_WINDOW_FUNCTION,
        PARAM_FREQUENCY_SCALE,
        PARAM_TIME_SMOOTHING,
        PARAM_FREQUENCY_SMOOTHING,
        PARAM_LOW_FREQUENCY,
        PARAM_HIGH_FREQUENCY,
        PARAM_SLOPE,
        PARAM_COLOR_FLOOR,
        PARAM_COLOR_CEILING,
        PARAM_LINEAR_FLOOR,
        PARAM_LINEAR_CEILING,
        NUM_PARAMS
    };

    enum InputIds {
        INPUT_SIGNAL,
        NUM_INPUTS
    };

    enum OutputIds {
        NUM_OUTPUTS
    };

    enum LightIds {
        LIGHT_RUN,
        NUM_LIGHTS
    };

    /// A complete engine-published spectrum; revisions also locate the scan line.
    struct DisplayColumn {
        std::array<float, N_FFT / 2 + 1> values{};
        uint64_t revision = 0;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        int64_t endpoint = -1, published_at = -1;
#endif
    };

 private:
    /// The sample rate of the module.
    float sample_rate = 0.f;

    /// Double feedback state avoids accumulated DC bias from float rounding
    /// in short periodic inputs. Analysis and display storage remain float.
    Fourier::DCBlocker<double> dc_blocker;

    /// Engine-owned one-hop analyzer; all transform storage is prepared once.
    Fourier::SpectrumAnalysis<float> analysis{N_FFT, N_FFT / 2};

    /// Preallocated mailboxes retain the newest value of every history column.
    /// Only process/reset publishes; only the module's display consumes.
    std::unique_ptr<Fourier::DisplayMailbox<DisplayColumn>[]> display_columns{
        new Fourier::DisplayMailbox<DisplayColumn>[N_STFT]};
    /// Monotonic engine sequence for scan-line placement; never serialized.
    uint64_t display_revision = 0;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
    int64_t benchmark_now = 0, benchmark_endpoint = -1;
#endif

    /// @brief Publish one column without exposing mutable engine storage.
    void publish_column(size_t index) {
        auto& column = display_columns[index].writable();
        column.revision = ++display_revision;
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        column.endpoint = benchmark_endpoint;
        column.published_at = benchmark_now;
#endif
        display_columns[index].publish();
    }

    /// The index of the current STFT hop.
    uint32_t hop_index = 0;

    /// A clock divider for updating the lights every 512 engine samples.
    Fourier::TriggerDivider light_divider;

    /// A Schmitt trigger for handling presses on the run button.
    Fourier::ThresholdTrigger<float> run_trigger;

    /// Whether the analyzer is running or not.
    bool is_running = true;

 public:
    /// Whether to apply AC coupling to input signal.
    bool is_ac_coupled = true;

    /// The color map to use when rasterizing STFT coefficients to images.
    Fourier::ColorMap::Function color_map = Fourier::ColorMap::Function::Magma;

    using Intensity = Fourier::SpectrogramIntensity;
    /// UI render preference; engine processing never reads intensity controls.
    Intensity::Scale intensity_scale = Intensity::Scale::Decibels;

    /// @brief Range quantities share the same controls in dB and amplitude modes.
    struct IntensityQuantity : ParamQuantity {
        bool linear() const { return paramId >= PARAM_LINEAR_FLOOR; }
        bool ceiling() const { return paramId == PARAM_COLOR_CEILING || paramId == PARAM_LINEAR_CEILING; }
        float sibling() {
            return Intensity::endpoint(module->params[paramId + (ceiling() ? -1 : 1)].getValue(),
                !ceiling(), linear());
        }
        float getMinValue() override {
            return ceiling() ? std::max(minValue, sibling() + (linear() ? 0.001f : 0.1f)) : minValue;
        }
        float getMaxValue() override {
            return ceiling() ? maxValue : std::min(maxValue, sibling() - (linear() ? 0.001f : 0.1f));
        }
        float getValue() override {
            const float value = Intensity::endpoint(ParamQuantity::getValue(), ceiling(), linear());
            return ceiling() ? value : Intensity::ordered_floor(value, sibling(), linear());
        }
        void fromJson(json_t* rootJ) override {
            const auto value = json_object_get(rootJ, "value");
            // Full deserialization repairs pairs only after both entries load,
            // so JSON array order does not change the retained range.
            const double raw = json_is_number(value) ? json_number_value(value) : defaultValue;
            module->params[paramId].setValue(static_cast<float>(std::max(double(minValue),
                std::min(double(maxValue), raw))));
        }
        std::string getDisplayValueString() override {
            return Intensity::format(getValue() * (linear() ? 100.f : 1.f));
        }
        std::string getDescription() override {
            return "Drag vertically to set the color range. Limits cannot cross. "
                "Recolors retained history, including while frozen. "
                "Raise the floor to hide weak background detail.";
        }
    };

    /// @brief Sanitized endpoints leave engine-owned analysis data untouched.
    float color_floor() { return Intensity::ordered_floor(params[PARAM_COLOR_FLOOR].getValue(),
        params[PARAM_COLOR_CEILING].getValue()); }
    float color_ceiling() { return Intensity::endpoint(params[PARAM_COLOR_CEILING].getValue(), true); }
    float linear_floor() { return Intensity::ordered_floor(params[PARAM_LINEAR_FLOOR].getValue(),
        params[PARAM_LINEAR_CEILING].getValue(), true); }
    float linear_ceiling() { return Intensity::endpoint(params[PARAM_LINEAR_CEILING].getValue(), true, true); }

    /// @brief Initialize a new spectrogram.
    Spectrogram() : sample_rate(APP->engine->getSampleRate()) {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        // Setup the input signal port and controls.
        configParam(PARAM_INPUT_GAIN, 0, std::pow(10.f, 12.f / 20.f), std::pow(10.f, 6.f / 20.f), "Input Gain", " dB", -10, 20);
        configInput(INPUT_SIGNAL, "Signal");
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
        // Setup the discrete frequency scale selector.
        configSwitch(PARAM_FREQUENCY_SCALE, 0, frequency_scale_names().size() - 1, static_cast<size_t>(FrequencyScale::Logarithmic), "Freq Scale", frequency_scale_names());
        getParamQuantity(PARAM_FREQUENCY_SCALE)->description =
            "The frequency-axis scale on the display. The DFT spaces\n"
            "frequencies linearly but humans hear frequencies along\n"
            "a logarithmic scale.";
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
        configParam<IntensityQuantity>(PARAM_COLOR_FLOOR, -120.f, 23.9f, -90.f, "Floor", " dB");
        configParam<IntensityQuantity>(PARAM_COLOR_CEILING, -119.9f, 24.f, 0.f, "Ceiling", " dB");
        configParam<IntensityQuantity>(PARAM_LINEAR_FLOOR, 0.f, 1.999f, 0.f, "Linear floor", "%", 0.f, 100.f);
        configParam<IntensityQuantity>(PARAM_LINEAR_CEILING, 0.001f, 2.f, 1.f, "Linear ceiling", "%", 0.f, 100.f);
        // Disable randomization for all parameters.
        for (size_t i = 0; i < NUM_PARAMS; i++)
            getParamQuantity(i)->randomizeEnabled = false;
        onReset();
    }

    /// @brief Respond to the module being reset by the host environment.
    inline void onReset() final {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        benchmark_endpoint = -1;
#endif
        Module::onReset();
        // Reset instance state of the module and menu preferences.
        is_running = true;
        hop_index = 0;
        is_ac_coupled = true;
        color_map = Fourier::ColorMap::Function::Magma;
        intensity_scale = Intensity::Scale::Decibels;
        // Rack resets quantities sequentially. Restore both pairs together so
        // a previous quiet ceiling cannot constrain the floor's reset value.
        params[PARAM_COLOR_FLOOR].setValue(-90.f);
        params[PARAM_COLOR_CEILING].setValue(0.f);
        params[PARAM_LINEAR_FLOOR].setValue(0.f);
        params[PARAM_LINEAR_CEILING].setValue(1.f);
        // Reset is a host lifecycle operation; publish cleared history columns.
        analysis.reset();
        for (size_t i = 0; i < N_STFT; ++i) {
            display_columns[i].writable().values.fill(0.f);
            publish_column(i);
        }
        // Act as if the sample rate has changed to reset remaining state.
        onSampleRateChange();
    }

    /// @brief Respond to a change in sample rate from the engine.
    inline void onSampleRateChange() final {
        Module::onSampleRateChange();
        sample_rate = APP->engine->getSampleRate();
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
        dc_blocker.setTransitionWidth(10.0, sample_rate);
        dc_blocker.reset();
    }

    // -----------------------------------------------------------------------
    // MARK: Serialization
    // -----------------------------------------------------------------------

    /// @brief Convert the module's state to a JSON object.
    /// @returns a pointer to a new json_t object with the module's state.
    inline json_t* dataToJson() final {
        json_t* rootJ = json_object();
        json_object_set_new(rootJ, "is_running", json_boolean(is_running));
        json_object_set_new(rootJ, "is_ac_coupled", json_boolean(is_ac_coupled));
        json_object_set_new(rootJ, "color_map", json_integer(static_cast<int>(color_map)));
        json_object_set_new(rootJ, "intensity_scale", json_string(
            intensity_scale == Intensity::Scale::Decibels ? "decibels" : "legacy_linear"));
        return rootJ;
    }

    /// @brief Reset only newly added state before Rack applies a complete patch.
    /// Missing params/data in old presets must not retain a previous edit.
    void fromJson(json_t* rootJ) override {
        params[PARAM_COLOR_FLOOR].setValue(-90.f);
        params[PARAM_COLOR_CEILING].setValue(0.f);
        params[PARAM_LINEAR_FLOOR].setValue(0.f);
        params[PARAM_LINEAR_CEILING].setValue(1.f);
        intensity_scale = Intensity::Scale::Linear;
        Module::fromJson(rootJ);
        // Rack's paramsFromJson bypasses ParamQuantity::fromJson. Reapply the
        // endpoint entries through our validator to default malformed values.
        size_t index;
        json_t* param;
        json_array_foreach(json_object_get(rootJ, "params"), index, param) {
            auto id_json = json_object_get(param, "id");
            if (!id_json) id_json = json_object_get(param, "paramId");
            const json_int_t id = id_json ? json_integer_value(id_json) : index;
            if (id >= PARAM_COLOR_FLOOR && id <= PARAM_LINEAR_CEILING)
                getParamQuantity(id)->fromJson(param);
        }
        params[PARAM_COLOR_FLOOR].setValue(color_floor());
        params[PARAM_LINEAR_FLOOR].setValue(linear_floor());
    }

    /// @brief Load the module's state from a JSON object.
    /// @param rootJ a pointer to a json_t with state data for this module.
    inline void dataFromJson(json_t* rootJ) final {
        const auto scale = json_object_get(rootJ, "intensity_scale");
        intensity_scale = json_is_string(scale) &&
            std::string(json_string_value(scale)) == "decibels" ?
            Intensity::Scale::Decibels : Intensity::Scale::Linear;
        json_t* opt = nullptr;
        if ((opt = json_object_get(rootJ, "is_running")))
            is_running = json_boolean_value(opt);
        if ((opt = json_object_get(rootJ, "is_ac_coupled")))
            is_ac_coupled = json_boolean_value(opt);
        color_map = Fourier::ColorMap::Function::Magma;
        opt = json_object_get(rootJ, "color_map");
        if (json_is_integer(opt)) {
            // Validate before narrowing so large saved integers cannot wrap.
            const json_int_t value = json_integer_value(opt);
            if (value >= 0 &&
                value < static_cast<json_int_t>(Fourier::ColorMap::Function::NumFunctions))
                color_map = static_cast<Fourier::ColorMap::Function>(value);
        }
    }

    // -----------------------------------------------------------------------
    // MARK: Parameters
    // -----------------------------------------------------------------------

    /// @brief Return the current sample rate of the module.
    /// @returns The sample rate of the module.
    inline const float& get_sample_rate() const { return sample_rate; }

    /// @brief Return the current hop index of the STFT.
    /// @returns The current hop index in [0, N_STFT - 1].
    inline const uint32_t& get_hop_index() const { return hop_index; }

#ifdef FOURIER_BENCHMARK_OBSERVABILITY
    /// @brief Benchmark replay only, after the engine barrier; never a UI read.
    uint64_t benchmark_publications() const { return display_revision; }
#endif
    /// @brief UI-only: acquire a newly published history column, if available.
    const DisplayColumn* consume_display_column(size_t index, bool include_current = false) {
        if (const auto column = display_columns[index].consume()) return column;
        return include_current ? &display_columns[index].current() : nullptr;
    }

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

    // Hop Length

    /// @brief Return the hop length of the windowed DFT in samples.
    /// @returns The number of samples to hop between computations of the DFT.
    inline size_t get_hop_length() {
        return N_FFT >> 1;  // N_FFT / 2
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
        const float hop_time = get_hop_length() / sample_rate;
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

    /// @brief Process input signal.
    inline float process_input_signal() {
        // Sum input voices and normalize by 5 V, without clipping.
        auto signal = Fourier::Eurorack::fromAC(inputs[INPUT_SIGNAL].getVoltageSum());
        // Determine the gain to apply to this channel's input signal.
        const auto gain = params[PARAM_INPUT_GAIN].getValue();
        // Pass signal through the DC blocking filter. Do this regardless
        // of whether we are in AC-coupling mode to ensure when switching
        // between modes there is no graphical delay from the filter
        // accumulating signal data.
        dc_blocker.process(signal);
        // If AC coupling is enabled, replace signal with DC blocker output.
        if (is_ac_coupled) signal = static_cast<float>(dc_blocker.getValue());
        // Return the gain-adjusted sample for the analyzer to buffer.
        return gain * signal;
    }

    /// @brief Write scheduled bins directly into the next history column.
    inline void process_coefficients(float input) {
        if (analysis.is_frame_start()) {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
            benchmark_endpoint = benchmark_now;
#endif
            Fourier::SpectrumSettings settings;
            settings.length = N_FFT;
            settings.hop = get_hop_length();
            settings.window = get_window_function();
            settings.sample_rate = sample_rate;
            const auto smoothing = get_frequency_smoothing();
            settings.octave = smoothing == FrequencySmoothing::None ? 0.f : to_float(smoothing);
            settings.alpha = get_time_smoothing_alpha();
            analysis.configure(settings);
        }
        if (analysis.process(input, [this](size_t bin, float value) {
            display_columns[hop_index].writable().values[bin] = value;
        })) {
            publish_column(hop_index);
            hop_index = (hop_index + 1) % N_STFT;
        }
    }

    /// @brief Process a sample.
    /// @param args the sample arguments (sample rate, sample time, etc.)
    void process(const ProcessArgs& args) final {
#ifdef FOURIER_BENCHMARK_OBSERVABILITY
        benchmark_now = args.frame;
#endif
        // Handle presses to the run button.
        if (run_trigger.process(params[PARAM_RUN].getValue()))
            is_running = !is_running;
        // Process the input signal and advance STFT analysis while running.
        if (is_running) {
            process_coefficients(process_input_signal());
        }
        // Update the panel lights.
        if (light_divider.process()) {
            const auto light_time = args.sampleTime * light_divider.getDivision();
            lights[LIGHT_RUN].setSmoothBrightness(is_running, light_time);
        }
    }
};

/// A widget that displays an image stored in a 32-bit RGBA pixel buffer.
struct SpectralImageDisplay : TransparentWidget {
 private:
    /// The vertical (top) padding for the plot.
    const size_t pad_top = 20;
    /// The vertical (bottom) padding for the plot.
    const size_t pad_bottom = 50;
    /// The horizontal (left) padding for the plot.
    const size_t pad_left = 40;
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

    /// The spectrogram module to render data from.
    Spectrogram* module = nullptr;

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

    /// UI-owned coefficient history, copied from complete mailbox snapshots.
    Fourier::STFTCoefficients display_coefficients;
    uint64_t display_revision = 0;
    size_t display_hop = 0;
    bool image_dirty = true;
    std::array<bool, Spectrogram::N_STFT> dirty_columns{};
    std::array<float, Spectrogram::N_FFT / 2> row_gain{};
    std::array<float, Spectrogram::N_FFT / 2> row_position{};
    /// Pixel key includes mode and sanitized endpoints (ignored in legacy mode).
    std::array<float, 7> image_settings{};

    /// @brief Consume the latest snapshot of each retained history column.
    void sync_history(bool include_current = false) {
        for (size_t i = 0; i < display_coefficients.size(); ++i) {
            if (const auto column = module->consume_display_column(i, include_current)) {
                std::copy(column->values.begin(), column->values.end(), display_coefficients[i].begin());
                if (column->revision > display_revision) {
                    display_revision = column->revision;
                    display_hop = (i + 1) % display_coefficients.size();
                }
                image_dirty = true;
                dirty_columns[i] = true;
            }
        }
    }

    /// The pixels being rendered on the display.
    std::vector<uint8_t> pixels;

    /// The NanoVG image handle, or zero when no texture has been created.
    int screen = 0;
    /// The live context that owns screen. Accessed only on the UI thread.
    NVGcontext* screen_context = nullptr;

    /// @brief Release the texture while its owning context is still alive.
    /// @details Called before context destruction or ordinary widget deletion.
    /// Clearing both fields makes subsequent cleanup safe and drawing lazy.
    void release_screen() {
        if (screen > 0) nvgDeleteImage(screen_context, screen);
        screen = 0;
        screen_context = nullptr;
    }

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
    explicit SpectralImageDisplay(Spectrogram* module_) :
        TransparentWidget(), module(module_),
        display_coefficients(module_ ? Spectrogram::N_STFT : 0,
            Fourier::DFTCoefficients(module_ ? Spectrogram::N_FFT / 2 + 1 : 0, 0.f)) {
        if (module) sync_history(true);
        axes_cache = new Fourier::CachedDisplay([this](const DrawArgs& args) { draw_axes(args); });
        addChild(axes_cache);
    }

    ~SpectralImageDisplay() override {
        release_screen();
    }

    /// @brief Drop GPU resources before Rack destroys the graphics context.
    /// @details Keep analysis history intact; the next draw recreates the image
    /// in its new context, including when the analyzer is frozen.
    void onContextDestroy(const ContextDestroyEvent& e) override {
        release_screen();
        TransparentWidget::onContextDestroy(e);
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

    /// @brief Draw the Y ticks with a linear scale.
    /// @param args the arguments for the current draw call
    void draw_y_ticks_linear(const DrawArgs& args) {
        static constexpr float xticks = 10;
        for (float i = 1; i < xticks; i++) {
            // Determine the relative position and re-scale it to the pixel
            // location on-screen. Since we're drawing a static number of
            // points, the position doesn't change relative to the minimum or
            // maximum frequencies (only the label value will change.)
            float position = i / xticks;
            float point_y = rescale(position, 1.f, 0.f, pad_top, box.size.y - pad_bottom);
            // Render tick label
            float freq = get_low_frequency() + (get_high_frequency() - get_low_frequency()) * position;
            const auto freq_string = Fourier::freq_to_string(freq);
            nvgFontSize(args.vg, axis_font_size);
            nvgFillColor(args.vg, axis_font_color);
            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            axes_cache->add_label(Vec(pad_left - 3 * axis_stroke_width, point_y), freq_string, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }
    }

    /// @brief Draw the Y ticks with an exponential scale.
    /// @param args the arguments for the current draw call
    void draw_y_ticks_logarithmic(const DrawArgs& args) {
        // Use the spectrogram image height (number of vertical pixels)
        const int height = Spectrogram::N_FFT / 2;
        const float nyquist_rate = module->get_sample_rate() / 2.f;
        // Compute the mapping parameters using the same transformation as draw_spectrogram.
        // These define the portion of the texture that is used for the desired frequency range.
        float texture_y_low  = height * (1 - sqrt(get_low_frequency() / nyquist_rate));
        float texture_y_high = height * (1 - sqrt(get_high_frequency() / nyquist_rate));
        float image_section_height = texture_y_low - texture_y_high;
        float draw_height = box.size.y - pad_top - pad_bottom;
        float scale_y = draw_height / image_section_height;
        // Determine the frequency range in the logarithmic domain.
        const auto min_exponent = log10(fmax(100.f, get_low_frequency()));
        const auto max_exponent = log10(get_high_frequency());
        // Iterate over base frequencies (exponential steps).
        for (float exponent = min_exponent; exponent < max_exponent; exponent++) {
            float base_frequency = powf(10.f, exponent);
            // Compute the texture coordinate for this frequency using the same sqrt mapping.
            float t = height * (1 - sqrt(base_frequency / nyquist_rate));
            // Apply the same translation and scaling as used in draw_spectrogram.
            float point_y = pad_top + (t - texture_y_high) * scale_y;
            // Render the tick label.
            const auto freq_string = Fourier::freq_to_string(base_frequency);
            nvgFontSize(args.vg, axis_font_size);
            nvgFillColor(args.vg, axis_font_color);
            nvgTextAlign(args.vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
            axes_cache->add_label(Vec(pad_left - 3 * axis_stroke_width, point_y), freq_string, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        }
    }

    /// @brief Draw the spectrogram.
    /// @param args the arguments for the current draw call
    void draw_spectrogram(const DrawArgs& args) {
        // The reference frequency for the slope compensation.
        static constexpr float reference_frequency = 1000.f;
        const auto slope = module->get_slope();
        // Determine the Nyquist rate from the sample rate.
        const float nyquist_rate = module->get_sample_rate() / 2.f;
        // Determine the dimensions of the spectral image.
        const int width = display_coefficients.size();
        const int height = Spectrogram::N_FFT / 2;

        sync_history();
        const bool decibels = module->intensity_scale == Spectrogram::Intensity::Scale::Decibels;
        const std::array<float, 7> settings{{slope, nyquist_rate,
            static_cast<float>(module->get_frequency_scale()),
            static_cast<float>(module->color_map), static_cast<float>(module->intensity_scale),
            decibels ? module->color_floor() : module->linear_floor(),
            decibels ? module->color_ceiling() : module->linear_ceiling()}};
        if (settings != image_settings) {
            image_dirty = true;
            dirty_columns.fill(true);
            for (int y = 0; y < height; ++y) {
                auto gain = log2f((y / static_cast<float>(height)) * nyquist_rate /
                    reference_frequency + std::numeric_limits<float>::epsilon());
                row_gain[y] = Fourier::decibels2amplitude(slope * gain);
                row_position[y] = module->get_frequency_scale() == FrequencyScale::Logarithmic
                    ? height * Fourier::squared(static_cast<float>(y) / height) : y;
            }
        }
        // Image handles belong to the context that created them.
        if (screen_context && screen_context != args.vg) release_screen();
        if (image_dirty) {
            pixels.resize(height * width * 4);
            for (int x = 0; x < width; ++x) {
                if (!dirty_columns[x]) continue;
                for (int y = 0; y < height; ++y) {
                    float position;
                    if (decibels) {
                        const float db = Spectrogram::Intensity::color_db(display_coefficients[x],
                            row_position[y], module->get_sample_rate(), Spectrogram::N_FFT, slope);
                        position = Spectrogram::Intensity::position(db, settings[5], settings[6]);
                    } else {
                        auto coeff = row_gain[y] * Fourier::interpolate_coefficients(
                            display_coefficients[x], row_position[y]);
                        position = Spectrogram::Intensity::position(abs(coeff) / height, settings[5], settings[6], true);
                    }
                    auto color = Fourier::ColorMap::color_map(module->color_map, position);
                    const int index = 4 * (width * (height - 1 - y) + x);
                    pixels[index + 0] = color.r * 255;
                    pixels[index + 1] = color.g * 255;
                    pixels[index + 2] = color.b * 255;
                    pixels[index + 3] = 255;
                }
            }
        }
        if (screen == 0) {
            screen = nvgCreateImageRGBA(args.vg, width, height, 0, pixels.data());
            // Keep the pending image dirty so failed creation can be retried.
            if (screen == 0) return;
            screen_context = args.vg;
        } else if (image_dirty) {
            nvgUpdateImage(args.vg, screen, pixels.data());
        }
        image_settings = settings;
        image_dirty = false;
        dirty_columns.fill(false);

        // Compute the mask rectangle from the padded region.
        const Rect mask = Rect(
            Vec(pad_left, pad_top),
            box.size.minus(Vec(pad_left + pad_right, pad_top + pad_bottom))
        );

        // Compute transformation parameters based on frequency bounds
        float texture_y_low, texture_y_high;
        if (module->get_frequency_scale() == FrequencyScale::Logarithmic) {
            texture_y_low  = height * (1 - sqrt(get_low_frequency() / nyquist_rate));
            texture_y_high = height * (1 - sqrt(get_high_frequency() / nyquist_rate));
        } else {
            texture_y_low  = height * (1 - get_low_frequency() / nyquist_rate);
            texture_y_high = height * (1 - get_high_frequency() / nyquist_rate);
        }
        float image_section_height = texture_y_low - texture_y_high;
        float scale_y = mask.size.y / image_section_height;

        // Draw the spectrogram image within the mask
        nvgSave(args.vg);
        nvgScissor(args.vg, mask.pos.x, mask.pos.y, mask.size.x, mask.size.y);
        nvgSave(args.vg);
        // Translate so that the texture coordinate corresponding to the high frequency maps to mask.pos.y.
        nvgTranslate(args.vg, 0, mask.pos.y - texture_y_high * scale_y);
        // Scale vertically so that the selected frequency band fills the mask.
        nvgScale(args.vg, 1.0f, scale_y);
        nvgBeginPath(args.vg);
        // Draw the spectrogram image using the mask's x position and width.
        nvgRect(args.vg, mask.pos.x, 0, mask.size.x, height);
        nvgFillPaint(args.vg, nvgImagePattern(args.vg, mask.pos.x, 0, mask.size.x, height, 0, screen, 1.0));
        nvgFill(args.vg);
        nvgRestore(args.vg);
        nvgResetScissor(args.vg);
        nvgRestore(args.vg);

        // Draw a scan-line to indicate the current hop index.
        nvgBeginPath(args.vg);
        float scan_x = display_hop / static_cast<float>(width);
        nvgMoveTo(args.vg, mask.pos.x + scan_x * mask.size.x, mask.pos.y);
        nvgLineTo(args.vg, mask.pos.x + scan_x * mask.size.x, mask.pos.y + mask.size.y);
        nvgStrokeWidth(args.vg, axis_stroke_width);
        nvgStrokeColor(args.vg, axis_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
    }

    /// @brief Draw the mouse position cross-hair.
    /// @param args the arguments for the current draw call.
    void draw_cross_hair(const DrawArgs& args) {
        const auto mouse_position = get_mouse_position();
        // Convert normalized mouse y (0 = bottom, 1 = top) to a pixel coordinate.
        float y_pixels = rescale(mouse_position.y, 0, 1, box.size.y - pad_bottom, pad_top);
        // Draw the horizontal cross-hair.
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, pad_left, y_pixels);
        nvgLineTo(args.vg, box.size.x - pad_right, y_pixels);
        nvgStrokeWidth(args.vg, 0.5);
        nvgStrokeColor(args.vg, cross_hair_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // Draw the vertical cross-hair (always a linear mapping).
        float x_position = rescale(mouse_position.x, 0, 1, pad_left, box.size.x - pad_right);
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, x_position, pad_top);
        nvgLineTo(args.vg, x_position, box.size.y - pad_bottom);
        nvgStrokeWidth(args.vg, 0.5);
        nvgStrokeColor(args.vg, cross_hair_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
    }

    /// @brief Draw the cross-hair information as text.
    /// @param args the arguments for the current draw call.
    void draw_cross_hair_text(const DrawArgs& args) {
        const auto mouse_position = get_mouse_position();
        // Convert the mouse's normalized Y to a pixel coordinate.
        float y_pixels = rescale(mouse_position.y, 0, 1, box.size.y - pad_bottom, pad_top);
        float hover_freq = 0;

        if (module->get_frequency_scale() == FrequencyScale::Logarithmic) {
            // 'texHeight' is the height of the spectrogram texture.
            const int texHeight = Spectrogram::N_FFT / 2;
            const float nyquist = module->get_sample_rate() / 2.f;
            // Map the low/high frequency to texture coordinates using the square-root mapping.
            // (Flipping vertically: low frequency is at the bottom, high frequency at the top.)
            float texY_low  = texHeight * sqrt(get_low_frequency() / nyquist);
            float texY_high = texHeight * sqrt(get_high_frequency() / nyquist);
            // Compute the vertical scale factor from texture to screen.
            float scaleY = (box.size.y - pad_top - pad_bottom) / (texY_low - texY_high);
            float t = texY_high + (y_pixels - pad_top) / scaleY;
            hover_freq = nyquist * powf(t / texHeight, 2);
        } else {
            // Linear mapping.
            hover_freq = get_low_frequency() + (get_high_frequency() - get_low_frequency()) * mouse_position.y;
        }

        auto font_path = asset::plugin(plugin_instance, "res/Font/Arial/Bold.ttf");
        const std::shared_ptr<Font> font = APP->window->loadFont(font_path);
        nvgFontSize(args.vg, 9);
        nvgFontFaceId(args.vg, font->handle);
        nvgFillColor(args.vg, {{{0.f / 255.f, 90.f / 255.f, 11.f / 255.f, 1.f}}});
        nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_LEFT);

        // Render the hovered frequency at the top left.
        const auto freq_string = Fourier::freq_to_string(hover_freq);
        nvgText(args.vg, pad_left + 3, pad_top / 2, freq_string.c_str(), NULL);

        // Optionally, also render musical note information.
        if (hover_freq > 0) {
            Fourier::TunedNote note(hover_freq);
            nvgText(args.vg, pad_left + 55, pad_top / 2, note.note_string().c_str(), NULL);
            nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
            nvgText(args.vg, pad_left + 140, pad_top / 2, note.tuning_string().c_str(), NULL);
        }

        // Render the coefficient magnitude.
        // Map normalized coordinates to coefficient indices.
        int coeff_x = mouse_position.x * (display_coefficients.size() - 1);
        int coeff_y = Spectrogram::N_FFT * hover_freq / module->get_sample_rate();
        // Format and render the decibel value.
        std::ostringstream oss;
        if (module->intensity_scale == Spectrogram::Intensity::Scale::Decibels) {
            // The texture uses one column per time cell and one sample per row.
            // Report that row's fractional bin before NanoVG's texture filtering.
            const int height = Spectrogram::N_FFT / 2;
            coeff_x = std::min(int(mouse_position.x * display_coefficients.size()),
                int(display_coefficients.size()) - 1);
            const float bin = Spectrogram::N_FFT * hover_freq / module->get_sample_rate();
            const bool logarithmic = module->get_frequency_scale() == FrequencyScale::Logarithmic;
            const float row = std::min(float(height - 1), std::floor(logarithmic ?
                std::sqrt(bin / height) * height : bin));
            const float color_bin = Spectrogram::Intensity::row_bin(row, height, logarithmic);
            const float raw = Spectrogram::Intensity::decibels(
                display_coefficients[coeff_x][coeff_y].real(), height);
            const float weighted = Spectrogram::Intensity::color_db(display_coefficients[coeff_x],
                color_bin, module->get_sample_rate(), Spectrogram::N_FFT, module->get_slope());
            oss << "Raw " << Spectrogram::Intensity::format(raw) << " dB  Color "
                << Spectrogram::Intensity::format(weighted) << " dB";
        } else {
            const float coeff_value = abs(display_coefficients[coeff_x][coeff_y]);
            const float db = Fourier::amplitude2decibels(coeff_value) - 60.f;
            oss << std::fixed << std::setprecision(1) << db << " dB";
        }
        nvgTextAlign(args.vg, NVG_ALIGN_MIDDLE | NVG_ALIGN_RIGHT);
        nvgText(args.vg, box.size.x - pad_right - 3, pad_top / 2, oss.str().c_str(), NULL);
    }

    /// @brief Invalidate static artwork when its rendering inputs change.
    void prepare_axes_cache() {
        axes_cache->prepare(box.size, {{get_low_frequency(), get_high_frequency(),
            module ? module->get_sample_rate() : APP->engine->getSampleRate(),
            static_cast<float>(module ? module->get_frequency_scale() : FrequencyScale::Logarithmic), 0.f}});
    }

    /// @brief Rasterize the static background and cache frequency-label layout.
    void draw_axes(const DrawArgs& args) {
        // Background
        nvgBeginPath(args.vg);
        nvgRoundedRect(args.vg, 0, 0, box.size.x, box.size.y, corner_radius);
        nvgFillColor(args.vg, background_color);
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, axis_stroke_color);
        nvgStroke(args.vg);
        nvgClosePath(args.vg);
        // Spectrogram plot
        if (module != nullptr) {
            // draw ticks for the axes of the plot.
            switch (module->get_frequency_scale()) {
            case FrequencyScale::Linear:
                draw_y_ticks_linear(args);
                break;
            case FrequencyScale::Logarithmic:
                draw_y_ticks_logarithmic(args);
                break;
            default:
                throw std::runtime_error("Invalid frequency scale");
            }
        }
    }

    /// @brief Draw the display on the main context.
    /// @param args the arguments for the draw context for this widget
    void drawLayer(const DrawArgs& args, int layer) override {
        if (layer == 1) {  // draw regardless of brightness settings.
            prepare_axes_cache();
            axes_cache->draw_cached(args);
            const auto font = APP->window->loadFont(asset::plugin(plugin_instance, "res/Font/Arial/Bold.ttf"));
            if (font) axes_cache->draw_labels(args, font->handle, axis_font_size, axis_font_color);
            if (module != nullptr) {
                draw_spectrogram(args);
                // Interactive mouse hovering functionality.
                if (mouse_state.is_hovering) {
                    draw_cross_hair(args);
                    draw_cross_hair_text(args);
                }
            }
            // Border
            nvgBeginPath(args.vg);
            nvgRect(args.vg, pad_left, pad_top, box.size.x - pad_left - pad_right, box.size.y - pad_top - pad_bottom);
            nvgStrokeWidth(args.vg, axis_stroke_width);
            nvgStrokeColor(args.vg, axis_stroke_color);
            nvgStroke(args.vg);
            nvgClosePath(args.vg);
        }
        Widget::drawLayer(args, layer);
    }
};

/// @brief One endpoint of the vertical color range, using Rack parameter menus.
/// UI-only: immediate changes recolor frozen history without engine smoothing.
struct SpectreIntensityHandle : ParamWidget {
    float drag_start = 0.f;

    bool enabled() {
        auto spectre = dynamic_cast<Spectrogram*>(module);
        return spectre && linear() == (spectre->intensity_scale == Spectrogram::Intensity::Scale::Linear);
    }
    bool linear() const { return paramId >= Spectrogram::PARAM_LINEAR_FLOOR; }
    bool ceiling() const { return paramId == Spectrogram::PARAM_COLOR_CEILING ||
        paramId == Spectrogram::PARAM_LINEAR_CEILING; }

    /// @brief Map endpoints to the mode's fixed dB or normalized-amplitude axis.
    static float position(float value, bool linear = false) {
        return Fourier::PanelLayout::intensity_bar_top() +
            (linear ? (2.f - value) / 2.f : (24.f - value) / 144.f) *
            Fourier::PanelLayout::intensity_bar_height();
    }

    void step() override {
        const auto quantity = getParamQuantity();
        const float value = quantity ? quantity->getValue() :
            (linear() ? (ceiling() ? 1.f : 0.f) : (ceiling() ? 0.f : -90.f));
        const auto origin = Fourier::PanelLayout::intensity_control().pos;
        box = Rect(origin.plus(Vec(ceiling() ? 32.f : 0.f, position(value, linear()) - 10.f)), Vec(19.f, 20.f));
        visible = module ? enabled() : !linear();
        ParamWidget::step();
    }

    void onDragStart(const DragStartEvent& e) override {
        if (enabled() && e.button == GLFW_MOUSE_BUTTON_LEFT)
            drag_start = getParamQuantity()->getValue();
    }

    /// @brief Pixel-relative dragging with Rack's fine/faster modifier conventions.
    void drag_by(float pixels, int modifiers) {
        if (!enabled()) return;
        const int mods = modifiers & RACK_MOD_MASK;
        const float speed = mods == (RACK_MOD_CTRL | GLFW_MOD_SHIFT) ? 0.01f :
            (mods == RACK_MOD_CTRL ? 0.1f : (mods == GLFW_MOD_SHIFT ? 4.f : 1.f));
        auto quantity = getParamQuantity();
        quantity->setValue(quantity->getValue() - pixels * (linear() ? 2.f : 144.f) / Fourier::PanelLayout::intensity_bar_height() * speed);
    }

    void onDragMove(const DragMoveEvent& e) override {
        if (e.button == GLFW_MOUSE_BUTTON_LEFT)
            drag_by(e.mouseDelta.y / getAbsoluteZoom(), APP->window->getMods());
    }

    void onDragEnd(const DragEndEvent& e) override {
        if (!enabled() || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        const float value = getParamQuantity()->getValue();
        if (value == drag_start) return;
        auto action = new history::ParamChange;
        action->name = ceiling() ? "change color ceiling" : "change color floor";
        action->moduleId = module->id;
        action->paramId = paramId;
        action->oldValue = drag_start;
        action->newValue = value;
        APP->history->push(action);
    }

    void draw(const DrawArgs& args) override {
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, ceiling() ? 0.f : 19.f, 10.f);
        nvgLineTo(args.vg, ceiling() ? 16.f : 3.f, 4.f);
        nvgLineTo(args.vg, ceiling() ? 16.f : 3.f, 16.f);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGB(235, 235, 235));
        nvgFill(args.vg);
        nvgStrokeColor(args.vg, nvgRGB(35, 35, 35));
        nvgStrokeWidth(args.vg, 0.7f);
        nvgStroke(args.vg);
    }
};

/// @brief Color screen with direct palette and scale menus, shared by both modes.
struct SpectreIntensityLegend : OpaqueWidget {
    Spectrogram* module;
    int hovered_choice = -1;

    explicit SpectreIntensityLegend(Spectrogram* module) : module(module) {}
    bool linear() const {
        return module && module->intensity_scale == Spectrogram::Intensity::Scale::Linear;
    }
    /// @brief Keep menu actions serialized and undoable through the Rack engine.
    void select_scale(size_t value) {
        if (!module) return;
        Fourier::set_module_setting(module, "change intensity scale", "intensity_scale",
            json_string(value == 0 ? "decibels" : "legacy_linear"));
    }
    void select_palette(size_t value) {
        if (!module) return;
        Fourier::set_module_setting(module, "change color map", "color_map", json_integer(value));
    }
    /// @brief Share precise hit regions between pointer feedback and activation.
    static int choice_at(Vec position) {
        for (int row = 0; row < 2; ++row)
            if (Fourier::PanelLayout::intensity_choice(row).contains(position)) return row;
        return -1;
    }
    void onHover(const HoverEvent& e) override {
        hovered_choice = module ? choice_at(e.pos) : -1;
        OpaqueWidget::onHover(e);
    }
    void onLeave(const LeaveEvent& e) override { hovered_choice = -1; OpaqueWidget::onLeave(e); }
    void onButton(const ButtonEvent& e) override {
        const int choice = choice_at(e.pos);
        if (module && e.button == GLFW_MOUSE_BUTTON_LEFT && e.action == GLFW_PRESS && choice >= 0) {
            auto menu = createMenu();
            // Put each list directly under its readout rather than nesting a submenu.
            const bool palette = choice == 0;
            const auto row = Fourier::PanelLayout::intensity_choice(choice);
            menu->box.pos = getAbsoluteOffset(Vec(row.pos.x, row.getBottomRight().y));
            const auto names = palette ? Fourier::ColorMap::names() : std::vector<std::string>{"Decibels", "Linear"};
            for (size_t i = 0; i < names.size(); ++i) {
                menu->addChild(createCheckMenuItem(names[i], "", [=]() {
                    return i == (palette ? static_cast<size_t>(module->color_map) :
                        static_cast<size_t>(module->intensity_scale));
                }, [=]() { if (palette) select_palette(i); else select_scale(i); }));
            }
            e.consume(this);
            return;
        }
        OpaqueWidget::onButton(e);
    }
    /// @brief Endpoints in the active mode's native units.
    std::array<std::string, 3> labels() {
        if (linear()) return {{Spectrogram::Intensity::format(module->linear_ceiling() * 100.f),
            Spectrogram::Intensity::format(module->linear_floor() * 100.f), "%"}};
        return {{Spectrogram::Intensity::format(module ? module->color_ceiling() : 0.f),
            Spectrogram::Intensity::format(module ? module->color_floor() : -90.f), "dB"}};
    }
    void draw(const DrawArgs& args) override {
        auto vg = args.vg;
        nvgBeginPath(vg);
        nvgRoundedRect(vg, 0.f, 0.f, box.size.x, box.size.y, 5.f);
        nvgFillColor(vg, nvgRGB(0, 0, 0));
        nvgFill(vg);
        const auto palette = module ? module->color_map : Fourier::ColorMap::Function::Magma;
        const float floor = module ? (linear() ? module->linear_floor() : module->color_floor()) : -90.f;
        const float ceiling = module ? (linear() ? module->linear_ceiling() : module->color_ceiling()) : 0.f;
        // Adjacent strips must meet without antialiased seams at fractional zoom.
        nvgSave(vg);
        nvgShapeAntiAlias(vg, 0);
        const int height = Fourier::PanelLayout::intensity_bar_height();
        for (int row = 0; row < height; ++row) {
            const float fraction = row / static_cast<float>(height - 1);
            const float value = linear() ? 2.f * (1.f - fraction) : 24.f - fraction * 144.f;
            const float position = Spectrogram::Intensity::position(value, floor, ceiling, linear());
            const auto color = Fourier::ColorMap::color_map(palette, position);
            nvgBeginPath(vg);
            nvgRect(vg, 19.f, Fourier::PanelLayout::intensity_bar_top() + row, 13.f, 1.f);
            nvgFillColor(vg, nvgRGBf(color.r, color.g, color.b));
            nvgFill(vg);
        }
        nvgRestore(vg);
        const auto font = APP->window->uiFont;
        if (!font) return;
        const auto text = labels();
        nvgFontFaceId(vg, font->handle);
        nvgFontSize(vg, 9.f);
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgFillColor(vg, nvgRGB(230, 230, 230));
        nvgText(vg, box.size.x / 2.f, 48.f, (text[0] + text[2]).c_str(), nullptr);
        nvgText(vg, box.size.x / 2.f, 182.f, (text[1] + text[2]).c_str(), nullptr);
        // Rack display choices align labels to a common left inset and reserve
        // a fixed right gutter. Keep this plugin's green-on-black treatment.
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        for (int choice = 0; choice < 2; ++choice) {
            const auto row = Fourier::PanelLayout::intensity_choice(choice);
            const float y = row.getCenter().y;
            const float right = row.getBottomRight().x - 2.f;
            nvgFillColor(vg, hovered_choice == choice ? nvgRGB(100, 255, 120) : nvgRGB(0, 215, 26));
            const auto label = choice == 0 ? Fourier::ColorMap::names()[static_cast<size_t>(palette)] :
                (linear() ? "Linear" : "Decibels");
            nvgText(vg, row.pos.x + 2.f, y, label.c_str(), nullptr);
            nvgBeginPath(vg);
            nvgMoveTo(vg, right - 4.f, y - 1.f);
            nvgLineTo(vg, right, y - 1.f);
            nvgLineTo(vg, right - 2.f, y + 1.5f);
            nvgFill(vg);
        }
    }
};

struct SpectrogramWidget : ModuleWidget {
    explicit SpectrogramWidget(Spectrogram* module) : ModuleWidget() {
        setModule(module);
        setPanel(new Fourier::Panel(Fourier::PanelKind::SPECTRE));
        // Spectrogram display
        SpectralImageDisplay* display = new SpectralImageDisplay(module);
        display->setPosition(Fourier::PanelLayout::display_position(Fourier::PanelKind::SPECTRE));
        display->setSize(Fourier::PanelLayout::display_size(Fourier::PanelKind::SPECTRE));
        addChild(display);
        // Inputs
        addInput(createInput<ThemedPJ301MPort>(Fourier::PanelLayout::spectre_input(), module, Spectrogram::INPUT_SIGNAL));
        addParam(createParamCentered<RoundSmallBlackKnob>(Fourier::PanelLayout::spectre_gain(), module, Spectrogram::PARAM_INPUT_GAIN));
        auto legend = new SpectreIntensityLegend(module);
        legend->box = Fourier::PanelLayout::intensity_control();
        addChild(legend);
        for (const int id : {Spectrogram::PARAM_COLOR_FLOOR, Spectrogram::PARAM_COLOR_CEILING,
                             Spectrogram::PARAM_LINEAR_FLOOR, Spectrogram::PARAM_LINEAR_CEILING}) {
            auto handle = createParam<SpectreIntensityHandle>(Vec(), module, id);
            addParam(handle);
        }
        // Buttons.
        addParam(createParamCentered<PB61303>(Fourier::PanelLayout::run(Fourier::PanelKind::SPECTRE), module, Spectrogram::PARAM_RUN));
        addChild(createLightCentered<PB61303Light<WhiteLight>>(Fourier::PanelLayout::run(Fourier::PanelKind::SPECTRE), module, Spectrogram::LIGHT_RUN));
        // Screen controls. Rack screenshots and browser previews have no module.
        // Supply display defaults without constructing an engine/DSP instance.
        // Window function control with custom angles to match discrete range.
        auto window_function_param = createParam<TextKnob>(Vec(80 + 0 * 61, 330), module, Spectrogram::PARAM_WINDOW_FUNCTION);
        window_function_param->label.text = "WINDOW";
        window_function_param->maxAngle = 2.f * M_PI;
        if (!module) window_function_param->value.text = "Flattop";
        addParam(window_function_param);
        // Frequency scale control with custom angles to match discrete range.
        auto frequency_scale_param = createParam<TextKnob>(Vec(80 + 1 * 61, 330), module, Spectrogram::PARAM_FREQUENCY_SCALE);
        frequency_scale_param->maxAngle = 0.3 * M_PI;
        frequency_scale_param->label.text = "FREQ SCALE";
        frequency_scale_param->label.font_size = 8.5f;
        if (!module) frequency_scale_param->value.text = "Logarithmic";
        addParam(frequency_scale_param);
        // Time smoothing control.
        auto time_smoothing_param = createParam<TextKnob>(Vec(80 + 2 * 61, 330), module, Spectrogram::PARAM_TIME_SMOOTHING);
        time_smoothing_param->label.text = "AVERAGE";
        if (!module) time_smoothing_param->value.text = "0ms";
        addParam(time_smoothing_param);
        // Frequency smoothing control with custom angles to match discrete range.
        auto frequency_smoothing_param = createParam<TextKnob>(Vec(80 + 3 * 61, 330), module, Spectrogram::PARAM_FREQUENCY_SMOOTHING);
        frequency_smoothing_param->label.text = "SMOOTH";
        frequency_smoothing_param->maxAngle = 2.f * M_PI;
        if (!module) frequency_smoothing_param->value.text = "None";
        addParam(frequency_smoothing_param);
        // Low and High frequency (frequency range) controls.
        auto low_freq_param = createParam<TextKnob>(Vec(80 + 4 * 61, 330), module, Spectrogram::PARAM_LOW_FREQUENCY);
        low_freq_param->label.text = "LO FREQ";
        if (!module) low_freq_param->value.text = "0Hz";
        addParam(low_freq_param);
        auto high_freq_param = createParam<TextKnob>(Vec(80 + 5 * 61, 330), module, Spectrogram::PARAM_HIGH_FREQUENCY);
        high_freq_param->label.text = "HI FREQ";
        if (!module) high_freq_param->value.text = string::f("%.5gHz", APP->engine->getSampleRate() / 2.f);
        addParam(high_freq_param);
        // Slope (dB/octave @1000Hz) controls.
        auto slope_param = createParam<TextKnob>(Vec(80 + 6 * 61, 330), module, Spectrogram::PARAM_SLOPE);
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
        auto module = getModule<Spectrogram>();
        menu->addChild(new MenuSeparator);
        menu->addChild(createMenuLabel("Render Settings"));
        menu->addChild(createBoolMenuItem("AC-coupled", "",
            [=]() { return module->is_ac_coupled; },
            [=](bool value) {
                Fourier::set_module_setting(module, "change ac-coupled",
                    "is_ac_coupled", json_boolean(value));
            }));
        ModuleWidget::appendContextMenu(menu);
    }
};

Model* modelSpectrogram = createModel<Spectrogram, SpectrogramWidget>("Spectrogram");
