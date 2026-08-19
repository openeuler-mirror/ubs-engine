# AGENTS.md

## Project Overview

UBS-Engine (UB Service Core Engine) - Software-defined computing with resource on-demand composition and allocation. C++17/C11 CMake project targeting openEuler Linux (ARM64).

## Build Commands

```shell
# Release build (default)
bash build.sh

# Debug build
bash build.sh -D

# Release with debug info
bash build.sh -T RelWithDebInfo

# Build and package as RPM
bash build.sh package

# Clean build directory
bash build.sh -c
```

## Test Commands

```shell
# Run all UT tests
bash build.sh ut

# Run specific test suite
bash build.sh ut -- --gtest_filter="Bandbridge*"

# Run specific test case
bash build.sh ut -- --gtest_filter="TestUbseMemControllerAddrApi.CheckAddrResourceStateExist"

# Generate coverage report
bash build.sh ut -C

# Coverage report + HTTP server
bash build.sh ut -C -H

# Build tests without running
bash build.sh ut --skip-run-tests
```

## Code Style

- C++17 / C11 standard
- clang-format for formatting (via pre-commit hooks)
- clang-tidy for static analysis (Release for src, Debug for test)
- Comments in Chinese or English as appropriate

## Dev Environment Tips

- Default build type is Release; tests auto-switch to Debug for coverage
- Build uses Ninja if available, falls back to Makefiles
- `compile_commands.json` is in build directory (e.g., `cmake-build-debug/`, `cmake-build-release/`)
- Point your LSP to the correct build directory for your configuration
- Auto-generated headers in `cmake-build-*/include/` (config.h, register_xalarm.h)
- Config files in `conf/`, copied to build directory during build
- Main executable: `ubse` daemon (from `src/ubse_main.cpp`)
- Shared libraries output to `cmake-build-*/lib`, executables to `cmake-build-*/bin`
- CI build uses `is_build_project=true` env var to skip local optimizations
- UB optimization enabled by default (`ENABLE_UB=ON`), disable with `--hccs`

## Architecture

```
src/
├── framework/      # Core framework (cert, com, config, context, event, ha, http, ipc, log, misc,
│                   #  plugin_mgr, security, serde, storage, thread_pool, timer, trace_context,
│                   #  vscok, xml)
├── controllers/    # Resource controllers (mem, node, npu, urma)
├── api_server/     # Northbound API
├── sdk/            # SDK for external consumers
├── cli/            # CLI interface (ubse_cli_framework, ubse_cert, ubse_cli_consumer)
├── adapter_plugins/# Plugin adapters (syssentry, mti, mmi, urma_uvs, bandbridge)
├── addons/         # Additional features (rmrs, virt_agent, ucache, process_mem)
├── message/        # Message handling
└── ras/            # Fault handling

test/
├── UT/             # Unit tests (GTest, per-module directories)
├── IT/             # Integration tests (scenarios, stubs, infra)
├── PT/             # Performance tests
└── tools/          # Test tools
```

## Security Guidelines

**Forbidden behaviors (red-line rules):**

1. **Committing sensitive information**
   - API keys, passwords, tokens, certificate private keys
   - Database connection strings containing credentials
   - SSH private keys, GPG keys

2. **Hardcoding credentials**
   - Username/password
   - Access Key/Secret Key
   - Authentication tokens

3. **Bypassing security checks**
   - Disabling SSL/TLS verification
   - Commenting out or deleting security-related code
   - Turning off authentication/authorization mechanisms

4. **Insecure logging**
   - Logging sensitive data (passwords, tokens, personal information)
   - Logging credentials in plaintext

**Must comply:**

- Manage credentials via environment variables or config files (config files must be `.gitignore`d)
- Sensitive operations require code review
- Rotate keys and credentials regularly
- Report security vulnerabilities without public disclosure

## Commit Guidelines

- Run `bash build.sh ut` before committing
- Ensure clang-format and clang-tidy checks pass
- Add or update tests for code changes
