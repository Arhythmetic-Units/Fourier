// Exercise Rack's module screenshot exporter with fresh graphics contexts.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later

#include "host.hpp"
#include <cmath>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <stb_image.h>
#include "../../src/Spectrogram.cpp"
#include "../../src/SpectrumAnalyzer.cpp"

Plugin* plugin_instance = nullptr;

/// @brief Check preview defaults against real parameters and exported glyphs.
void verify_preview(rack::plugin::Model* model, const std::string& filename,
                    int expected_controls) {
    int width = 0, height = 0, channels = 0;
    const auto bytes = rack::system::readFile(filename);
    std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
        stbi_load_from_memory(bytes.data(), bytes.size(), &width, &height, &channels, 4),
        stbi_image_free);
    if (!pixels) throw std::runtime_error("Cannot read " + filename);
    std::unique_ptr<rack::app::ModuleWidget> preview(model->createModuleWidget(nullptr));
    // ModuleWidget owns its module. This is test-only; previews need no DSP instance.
    auto module = model->createModule();
    APP->engine->addModule(module);
    std::unique_ptr<rack::app::ModuleWidget> live(model->createModuleWidget(module));
    preview->step();
    live->step();
    const float scale = width / preview->box.size.x;
    if (height != static_cast<int>(preview->box.size.y * scale) || scale < 1.f)
        throw std::runtime_error("Unexpected screenshot dimensions: " + filename);
    int controls = 0;
    for (auto child : preview->children) {
        auto knob = dynamic_cast<TextKnob*>(child);
        if (!knob) continue;
        ++controls;
        auto live_knob = dynamic_cast<TextKnob*>(live->getParam(knob->paramId));
        if (!live_knob) throw std::runtime_error("Missing live text control");
        auto quantity = live_knob->getParamQuantity();
        const auto expected = quantity->getDisplayValueString() + quantity->getUnit();
        if (knob->getParamQuantity() || knob->hovered || knob->value.text.empty() ||
            knob->value.text != expected || live_knob->value.text != expected)
            throw std::runtime_error(model->slug + " " + knob->label.text +
                ": expected " + expected + ", preview has '" + knob->value.text + "'");
        // Require green ink in each value row, excluding the label above it.
        // This catches missing glyphs even when the strings themselves are valid.
        int ink = 0;
        const int left = static_cast<int>(std::ceil(knob->box.pos.x * scale));
        const int right = static_cast<int>((knob->box.pos.x + knob->box.size.x) * scale);
        const int top = static_cast<int>(std::ceil((knob->box.pos.y + 18.f) * scale));
        const int bottom = static_cast<int>((knob->box.pos.y + knob->box.size.y) * scale);
        if (left < 0 || right > width || top < 0 || bottom > height)
            throw std::runtime_error("Text control escaped screenshot bounds");
        for (int y = top; y < bottom; ++y) {
            for (int x = left; x < right; ++x) {
                const auto pixel = pixels.get() + (y * width + x) * 4;
                if (pixel[1] > 100 && pixel[1] > pixel[0] * 2 && pixel[1] > pixel[2] * 2)
                    ++ink;
            }
        }
        if (ink < 8)
            throw std::runtime_error(model->slug + " " + knob->label.text +
                ": exported value row is blank");
        // Preview defaults must never replace changed live parameter values.
        quantity->setValue(quantity->minValue);
        live_knob->step();
        if (live_knob->value.text != quantity->getDisplayValueString() + quantity->getUnit())
            throw std::runtime_error("Live control stopped following its parameter");
    }
    if (controls != expected_controls) throw std::runtime_error("Missing text controls");
    std::cout << filename << ": " << controls << " preview values and glyph rows OK\n";
}

/// @brief Export both modules exactly as Rack's --screenshot option does.
int main(int argc, char** argv) {
    if (argc != 4) {
        std::cerr << "Usage: inspect_library_screenshots RACK_DIR PLUGIN_DIR OUTPUT_PREFIX\n";
        return 1;
    }
    rack::Context context;
    rack::contextSet(&context);
    context.engine = new rack::engine::Engine;
    context.event = new rack::widget::EventState;
    rack::asset::systemDir = argv[1];
    rack::plugin::Plugin plugin;
    plugin.path = argv[2];
    plugin.slug = "ArhythmeticUnits-Fourier";
    plugin.addModel(modelSpectrumAnalyzer);
    plugin.addModel(modelSpectrogram);
    rack::plugin::plugins.push_back(&plugin);
    plugin_instance = &plugin;
    if (!glfwInit()) return 2;
    int result = 0;
    try {
        for (const int sample_rate : {44100, 48000, 96000, 192000}) {
            context.engine->setSampleRate(sample_rate);
            context.window = new rack::window::Window;
            glfwHideWindow(context.window->win);
            context.scene = new rack::app::Scene;
            // Do not warm up glyphs or step live widgets before the export.
            // Rack skips existing PNGs, so remove only this check's two outputs.
            const auto directory = std::string(argv[3]) + "-" + std::to_string(sample_rate);
            const auto prefix = directory + "/" + plugin.slug + "/";
            for (auto model : plugin.models) {
                const auto filename = prefix + model->slug + ".png";
                rack::system::remove(filename);
                if (rack::system::isFile(filename))
                    throw std::runtime_error("Cannot remove stale screenshot: " + filename);
            }
            context.window->screenshotModules(directory);
            if (glGetError() != GL_NO_ERROR) throw std::runtime_error("OpenGL error during export");
            verify_preview(modelSpectrumAnalyzer, prefix + "SpectrumAnalyzer.png", 10);
            verify_preview(modelSpectrogram, prefix + "Spectrogram.png", 7);
            delete context.scene;
            context.scene = nullptr;
            delete context.window;
            context.window = nullptr;
        }
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        result = 3;
    }
    delete context.scene;
    context.scene = nullptr;
    delete context.window;
    context.window = nullptr;
    glfwTerminate();
    rack::plugin::plugins.clear();
    rack::contextSet(nullptr);
    return result;
}
