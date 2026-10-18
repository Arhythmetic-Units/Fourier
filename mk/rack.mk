# Headless suites share build flags; instrumentation stays out of the plugin.
RACK_TEST_INSTRUMENT ?=
RACK_TEST_BUILD := .build/test/rack
# Catch2 supplies main(), not the SDK's Windows Unicode entry point.
RACK_TEST_FLAGS := $(filter-out -std=% -municode,$(CXXFLAGS)) -std=c++14 -pthread -Idep/Catch2
ifneq ($(RACK_TEST_INSTRUMENT),)
RACK_TEST_BUILD := .build/instrumented/$(RACK_TEST_INSTRUMENT)/rack
# Clang does not support GCC's -fno-gnu-unique. Preserve SDK ABI and
# floating-point flags while keeping useful sanitizer stacks.
RACK_TEST_FLAGS := $(filter-out -fno-gnu-unique,$(RACK_TEST_FLAGS)) -g -fno-omit-frame-pointer
ifeq ($(RACK_TEST_INSTRUMENT),coverage)
# Keep complete inline mappings across the separate test executables.
RACK_TEST_FLAGS += -fprofile-instr-generate -fcoverage-mapping -femit-all-decls
else ifeq ($(RACK_TEST_INSTRUMENT),asan-ubsan)
RACK_TEST_FLAGS := $(filter-out -O%,$(RACK_TEST_FLAGS)) -O1 -fsanitize=address,undefined -fno-sanitize-recover=all
else
$(error RACK_TEST_INSTRUMENT must be coverage or asan-ubsan)
endif
endif

RACK_TEST_NAMES := test_serialization test_display_lifecycle test_spectrum_points test_dc_blocker test_module_amplitudes
# MinGW emits .exe files; name the actual targets to avoid needless rebuilds.
RACK_TEST_SUFFIX := $(if $(ARCH_WIN),.exe)
RACK_TEST_BINARIES := $(addprefix $(RACK_TEST_BUILD)/,$(addsuffix $(RACK_TEST_SUFFIX),$(RACK_TEST_NAMES)))
.PHONY: test-rack test-serialization test-display-lifecycle test-spectrum-points test-dc-blocker-simd test-module-amplitudes

test-rack: test-serialization test-display-lifecycle test-spectrum-points test-dc-blocker-simd test-module-amplitudes
test-serialization: $(RACK_TEST_BUILD)/test_serialization$(RACK_TEST_SUFFIX)
test-display-lifecycle: $(RACK_TEST_BUILD)/test_display_lifecycle$(RACK_TEST_SUFFIX)
test-spectrum-points: $(RACK_TEST_BUILD)/test_spectrum_points$(RACK_TEST_SUFFIX)
test-dc-blocker-simd: $(RACK_TEST_BUILD)/test_dc_blocker$(RACK_TEST_SUFFIX)
test-module-amplitudes: $(RACK_TEST_BUILD)/test_module_amplitudes$(RACK_TEST_SUFFIX)

test-serialization test-display-lifecycle test-spectrum-points test-dc-blocker-simd test-module-amplitudes:
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $<

$(RACK_TEST_BINARIES): $(RACK_TEST_BUILD)/%$(RACK_TEST_SUFFIX): $(RACK_TEST_BUILD)/%.cpp.o $(RACK_TEST_BUILD)/catch_amalgamated.cpp.o
	$(CXX) $(RACK_TEST_FLAGS) -o $@ $^ $(if $(filter test_dc_blocker,$*),,-L$(RACK_DIR) -lRack)

$(addprefix $(RACK_TEST_BUILD)/,$(addsuffix .cpp.o,$(RACK_TEST_NAMES))): $(RACK_TEST_BUILD)/%.cpp.o: test/rack/%.cpp Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(RACK_TEST_FLAGS) -c -o $@ $<

# Keep instrumented Catch2 objects separate from ordinary tests and benchmarks.
$(RACK_TEST_BUILD)/catch_amalgamated.cpp.o: dep/Catch2/catch_amalgamated.cpp Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(RACK_TEST_FLAGS) -c -o $@ $<

-include $(addprefix $(RACK_TEST_BUILD)/,$(addsuffix .cpp.d,$(RACK_TEST_NAMES))) $(RACK_TEST_BUILD)/catch_amalgamated.cpp.d

# CPU-side display preparation only; the instrumented renderer does not use GL.
.PHONY: benchmark-display
benchmark-display: .build/benchmark/rack/display$(RACK_TEST_SUFFIX)
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $<

.build/benchmark/rack/display$(RACK_TEST_SUFFIX): .build/benchmark/rack/display.cpp.o
	$(CXX) $(filter-out -municode,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack

.build/benchmark/rack/display.cpp.o: CXXFLAGS += $(DISPLAY_BENCHMARK_FLAGS)
-include .build/benchmark/rack/display.cpp.d

# Catch2 Rack benchmarks use SDK optimization and floating-point flags.
BENCHMARK_ARGS ?=
RACK_BENCHMARK_FLAGS = $(filter-out -std=% -municode,$(CXXFLAGS)) -std=c++14 -pthread -Idep/Catch2
RACK_BENCHMARK_NAMES := dsp coordinates graphics modules
RACK_BENCHMARK_OBJECTS := $(addprefix .build/benchmark/rack/,$(addsuffix .cpp.o,$(RACK_BENCHMARK_NAMES)))
RACK_BENCHMARK_BINARIES := $(addprefix .build/benchmark/rack/,$(addsuffix $(RACK_TEST_SUFFIX),$(RACK_BENCHMARK_NAMES)))
.PHONY: benchmark-dsp benchmark-coordinates benchmark-graphics benchmark-modules benchmark-rack-build
benchmark-rack-build: $(RACK_BENCHMARK_BINARIES)
benchmark-dsp: .build/benchmark/rack/dsp$(RACK_TEST_SUFFIX)
benchmark-coordinates: .build/benchmark/rack/coordinates$(RACK_TEST_SUFFIX)
benchmark-graphics: .build/benchmark/rack/graphics$(RACK_TEST_SUFFIX)
benchmark-modules: .build/benchmark/rack/modules$(RACK_TEST_SUFFIX)

benchmark-dsp benchmark-coordinates benchmark-graphics benchmark-modules:
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $< $(BENCHMARK_ARGS)

$(RACK_BENCHMARK_BINARIES): .build/benchmark/rack/%$(RACK_TEST_SUFFIX): .build/benchmark/rack/%.cpp.o .build/benchmark/rack/catch_amalgamated.cpp.o
	$(CXX) $(RACK_BENCHMARK_FLAGS) -o $@ $^ -L$(RACK_DIR) -lRack

$(RACK_BENCHMARK_OBJECTS): .build/benchmark/rack/%.cpp.o: benchmark/rack/%.cpp Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(RACK_BENCHMARK_FLAGS) -c -o $@ $<

.build/benchmark/rack/catch_amalgamated.cpp.o: dep/Catch2/catch_amalgamated.cpp Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(RACK_BENCHMARK_FLAGS) -c -o $@ $<

-include $(RACK_BENCHMARK_OBJECTS:.o=.d) .build/benchmark/rack/catch_amalgamated.cpp.d

# Optional native OpenGL inspection; requires a graphical desktop session.
ifdef ARCH_MAC
DISPLAY_GL_LIBS = -framework OpenGL
else ifdef ARCH_WIN
DISPLAY_GL_LIBS = -lopengl32
else
DISPLAY_GL_LIBS = -lGL
endif
.PHONY: inspect-displays
inspect-displays: .build/test/rack/inspect_displays$(RACK_TEST_SUFFIX)
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $< "$(abspath $(RACK_DIR))" "$(CURDIR)" "$(abspath .build/test/rack/display)"

.build/test/rack/inspect_displays$(RACK_TEST_SUFFIX): .build/test/rack/inspect_displays.cpp.o
	$(CXX) $(filter-out -municode,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack $(DISPLAY_GL_LIBS)

-include .build/test/rack/inspect_displays.cpp.d

.PHONY: inspect-panels
PANEL_INSPECT_BINARY := .build/test/rack/inspect_panels$(if $(ARCH_WIN),.exe)
inspect-panels: $(PANEL_INSPECT_BINARY)
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $< "$(abspath $(RACK_DIR))" "$(CURDIR)" "$(abspath .build/test/rack/panel)"

$(PANEL_INSPECT_BINARY): .build/test/rack/inspect_panels.cpp.o
	$(CXX) $(filter-out -municode,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack $(DISPLAY_GL_LIBS)

-include .build/test/rack/inspect_panels.cpp.d

# Use Rack's actual single-pass screenshot exporter, including null modules.
.PHONY: inspect-library-screenshots
inspect-library-screenshots: .build/test/rack/inspect_library_screenshots$(RACK_TEST_SUFFIX)
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $< "$(abspath $(RACK_DIR))" "$(CURDIR)" "$(abspath .build/test/rack/library)"

.build/test/rack/inspect_library_screenshots$(RACK_TEST_SUFFIX): .build/test/rack/inspect_library_screenshots.cpp.o
	$(CXX) $(filter-out -municode,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack $(DISPLAY_GL_LIBS)

.build/test/rack/inspect_library_screenshots.cpp.o: .build/rack-config
-include .build/test/rack/inspect_library_screenshots.cpp.d

# Optional research providers affect only the two paper executables.
PAPER_VDSP ?= 0
PAPER_FFTW_PREFIX ?=
PAPER_FLAGS :=
PAPER_LIBS :=
PAPER_FEATURES :=
PAPER_NATIVE_INPUTS :=
PAPER_IDENTITY_FLAGS = -DPAPER_RACK_LIBRARY=$(call shell-quote,"$(abspath $(firstword $(wildcard $(RACK_DIR)/libRack.*)))")
ifneq ($(strip $(PAPER_FFTW_PREFIX)),)
PAPER_FLAGS += -DPAPER_HAVE_FFTW -I"$(PAPER_FFTW_PREFIX)/include"
PAPER_LIBS += "$(PAPER_FFTW_PREFIX)/lib/libfftw3f.a" "$(PAPER_FFTW_PREFIX)/lib/libfftw3.a"
PAPER_NATIVE_INPUTS += $(PAPER_FFTW_PREFIX)/lib/libfftw3f.a $(PAPER_FFTW_PREFIX)/lib/libfftw3.a
PAPER_FEATURES += fftw
PAPER_IDENTITY_FLAGS += -DPAPER_FFTW_DIRECTORY=$(call shell-quote,"$(abspath $(PAPER_FFTW_PREFIX))")
endif

ifeq ($(PAPER_VDSP),1)
ifndef ARCH_MAC
$(error PAPER_VDSP=1 requires a macOS Rack SDK/toolchain)
endif
PAPER_FLAGS += -DPAPER_HAVE_VDSP
PAPER_LIBS += -framework Accelerate
PAPER_FEATURES += vdsp
else ifneq ($(PAPER_VDSP),0)
$(error PAPER_VDSP must be 0 or 1)
endif

# Raw paper observations, separate from Catch2's batched mean estimator.
.PHONY: benchmark-paper-build
benchmark-paper-build: .build/benchmark/rack/paper$(RACK_TEST_SUFFIX) .build/benchmark/rack/paper-audit$(RACK_TEST_SUFFIX)
benchmark-rack-build: benchmark-paper-build

.build/benchmark/rack/paper$(RACK_TEST_SUFFIX): .build/benchmark/paper/benchmark.cpp.o $(PAPER_NATIVE_INPUTS)
	@mkdir -p $(@D)
	$(CXX) $(filter-out -municode,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack $(PAPER_LIBS)

-include .build/benchmark/paper/benchmark.cpp.d

.build/benchmark/generate-registry$(RACK_TEST_SUFFIX): docs/whitepaper/benchmarks/lib/generate_registry.cpp .build/rack-config
	@mkdir -p $(@D)
	$(CXX) $(filter-out -municode -fno-gnu-unique,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack

.build/benchmark/registry.generated.hpp: docs/whitepaper/benchmarks/lib/backends.json .build/benchmark/generate-registry$(RACK_TEST_SUFFIX) .build/benchmark/paper-config
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" .build/benchmark/generate-registry$(RACK_TEST_SUFFIX) $< $@ "$(PAPER_FEATURES)"

.build/benchmark/paper/benchmark.cpp.o: benchmark/paper/benchmark.cpp .build/benchmark/registry.generated.hpp Makefile mk/rack.mk
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(PAPER_FLAGS) $(PAPER_IDENTITY_FLAGS) -I.build/benchmark -c -o $@ $<

.build/benchmark/paper/benchmark-audit.cpp.o: benchmark/paper/benchmark.cpp .build/benchmark/registry.generated.hpp Makefile mk/rack.mk .build/rack-config
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) $(PAPER_FLAGS) $(PAPER_IDENTITY_FLAGS) -I.build/benchmark -DPAPER_ALLOCATION_AUDIT -c -o $@ $<

.build/benchmark/rack/paper-audit$(RACK_TEST_SUFFIX): .build/benchmark/paper/benchmark-audit.cpp.o $(PAPER_NATIVE_INPUTS)
	@mkdir -p $(@D)
	$(CXX) $(filter-out -municode,$(CXXFLAGS)) -o $@ $< -L$(RACK_DIR) -lRack $(PAPER_LIBS)

-include .build/benchmark/paper/benchmark-audit.cpp.d

.build/benchmark/paper-config: PRIVATE_CONFIG := $(PAPER_FLAGS) $(PAPER_LIBS) $(PAPER_FEATURES) $(PAPER_IDENTITY_FLAGS)
.build/benchmark/paper/benchmark.cpp.o .build/benchmark/paper/benchmark-audit.cpp.o: .build/benchmark/paper-config

# Native development modes share a serial recipe, even with both goals and -j.
# Only the timing executable is needed; the publication audit build stays opt-in.
BENCHMARK_DEV_ARGS ?=
ifeq ($(origin BENCHMARK_DEV_OUT),undefined)
BENCHMARK_DEV_OUT := .build/benchmark-dev-$(shell date +%Y%m%d-%H%M%S)-$(shell echo $$$$)
endif
BENCHMARK_DEV_PROFILES = $(if $(filter benchmark-fast,$(MAKECMDGOALS)),fast) $(if $(filter benchmark-full,$(MAKECMDGOALS)),full)
BENCHMARK_DEV_MEASURES = $(if $(filter --list --help,$(BENCHMARK_DEV_ARGS)),,1)
.PHONY: benchmark-fast benchmark-full run-development-benchmarks benchmark-dev-build test-benchmark-dev
benchmark-dev-build: .build/benchmark/rack/paper$(RACK_TEST_SUFFIX)
benchmark-fast benchmark-full: run-development-benchmarks
run-development-benchmarks: .build/benchmark/rack/paper$(RACK_TEST_SUFFIX)
	@set -e; for profile in $(BENCHMARK_DEV_PROFILES); do \
		DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" \
		$(if $(and $(BENCHMARK_GUARD),$(BENCHMARK_DEV_MEASURES)),$(BENCHMARK_GUARD) --native-development-gate --record "$(BENCHMARK_DEV_OUT)-$$profile.guard.json" --) $< --development \
		--profile "$$profile" --output "$(BENCHMARK_DEV_OUT)-$$profile" $(BENCHMARK_DEV_ARGS); \
	done

test-benchmark-dev: $(RACK_TEST_BUILD)/test_benchmark_development$(RACK_TEST_SUFFIX)
	DYLD_LIBRARY_PATH="$(abspath $(RACK_DIR))" LD_LIBRARY_PATH="$(abspath $(RACK_DIR))" $<
test-rack: test-benchmark-dev

$(RACK_TEST_BUILD)/test_benchmark_development$(RACK_TEST_SUFFIX): $(RACK_TEST_BUILD)/test_benchmark_development.cpp.o $(RACK_TEST_BUILD)/catch_amalgamated.cpp.o
	$(CXX) $(RACK_TEST_FLAGS) -o $@ $^ -L$(RACK_DIR) -lRack

$(RACK_TEST_BUILD)/test_benchmark_development.cpp.o: test/rack/test_benchmark_development.cpp .build/benchmark/registry.generated.hpp Makefile mk/rack.mk $(RACK_TEST_BUILD)/config
	@mkdir -p $(@D)
	$(CXX) $(RACK_TEST_FLAGS) -I.build/benchmark -c -o $@ $<

-include $(RACK_TEST_BUILD)/test_benchmark_development.cpp.d

# Rebuild when the compiler, SDK path, or effective flags change. Stamps
# never enter a link command; only the objects depend on them.
.build/rack-config: PRIVATE_CONFIG := $(CXX) $(CXXFLAGS) $(LDFLAGS) $(DISPLAY_BENCHMARK_FLAGS) $(abspath $(RACK_DIR))
$(RACK_TEST_BUILD)/config: PRIVATE_CONFIG := $(CXX) $(RACK_TEST_FLAGS) $(abspath $(RACK_DIR))
.build/benchmark/rack/config: PRIVATE_CONFIG := $(CXX) $(RACK_BENCHMARK_FLAGS) $(abspath $(RACK_DIR))
.build/rack-config $(RACK_TEST_BUILD)/config .build/benchmark/rack/config .build/benchmark/paper-config: FORCE
	@mkdir -p $(@D)
	@printf '%s\n' $(call shell-quote,$(PRIVATE_CONFIG)) > $@.tmp
	@cmp -s $@.tmp $@ && rm $@.tmp || mv $@.tmp $@

$(OBJECTS) .build/benchmark/rack/display.cpp.o .build/benchmark/paper/benchmark.cpp.o .build/test/rack/inspect_displays.cpp.o .build/test/rack/inspect_panels.cpp.o: .build/rack-config
$(addprefix $(RACK_TEST_BUILD)/,$(addsuffix .cpp.o,$(RACK_TEST_NAMES))) $(RACK_TEST_BUILD)/catch_amalgamated.cpp.o: $(RACK_TEST_BUILD)/config
$(RACK_BENCHMARK_OBJECTS) .build/benchmark/rack/catch_amalgamated.cpp.o: .build/benchmark/rack/config
