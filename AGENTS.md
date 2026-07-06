# UBS-Engine

Software-defined computing, resource on-demand composition and allocation. C++17/C11 CMake project targeting openEuler Linux (ARM64).

## Build Commands

```shell
bash build.sh              # Release build (default)
bash build.sh -D           # Debug build
bash build.sh -T RelWithDebInfo  # Release with debug info
bash build.sh package      # Build and package as RPM to output/
```

## Test Commands

Tests require `BUILD_TESTS=ON`, automatically enabled when building test targets:

```shell
bash build.sh ut           # Run all UT tests (including bandbridge)
bash build.sh ut -- --gtest_filter="Bandbridge*"  # Run bandbridge kernel module UT only
bash build.sh ut -- --gtest_filter="TestSuite.*"  # Run specific tests
bash build.sh ut -C        # Generate coverage report (cmake-build-debug/coverage)
bash build.sh ut -C -H     # Coverage + start HTTP server to view report
```

## Architecture

- `src/framework/` - Core framework (com, config, event, ha, http, ipc, log, plugin_mgr, security, serde, thread_pool, timer, xml)
- `src/controllers/` - Resource controllers (mem pooling, node, urma)
- `src/api_server/` - Northbound API exposure
- `src/sdk/` - SDK for external consumers
- `src/cli/` - CLI interface (ubse_cli_framework, ubse_cert)
- `src/adapter_plugins/` - Plugin adapters (syssentry, mti, mmi)
- `src/ras/` - Fault handling
- `test/` - IT, PT, UT tests using GTest

## Key Conventions

- Main daemon executable: `ubse` (built from `src/ubse_main.cpp`)
- Shared libraries output to `cmake-build-*/lib`, executables to `cmake-build-*/bin`
- Auto-generated headers in `cmake-build-*/include/` (config.h, register_xalarm.h)
- Config files in `conf/`, copied to build dir during build
- CMake modules in `scripts/cmake/`
- Pre-commit hooks run `clang-format` and `clang-tidy` (release for src, debug for test)

## LSP Configuration

`compile_commands.json` is generated in the build directory (`cmake-build-debug`, `cmake-build-release`, etc.). Point LSP to the correct build dir based on your active configuration.

## Gotchas

- Default build type is Release; tests auto-switch to Debug for full coverage
- Local build uses Ninja if available, falls back to Makefiles
- CI build uses `is_build_project=true` env var to skip local optimizations
- UB optimization enabled by default (`ENABLE_UB=ON`), disable with `--hccs`
