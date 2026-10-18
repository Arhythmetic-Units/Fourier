// Host-only Rack declarations for regression and graphics inspection executables.
// Copyright 2026 Arhythmetic Units
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef ARHYTHMETIC_UNITS_FOURIER_TEST_RACK_HOST_HPP_
#define ARHYTHMETIC_UNITS_FOURIER_TEST_RACK_HOST_HPP_

// These executables construct a host; the shipped plugin must use rack.hpp.
// Read the host declarations before rack.hpp applies its PRIVATE restriction.
#include <engine/Engine.hpp>
#include <history.hpp>
#include <app/Scene.hpp>
#include <window/Window.hpp>
#undef PRIVATE

#endif  // ARHYTHMETIC_UNITS_FOURIER_TEST_RACK_HOST_HPP_
