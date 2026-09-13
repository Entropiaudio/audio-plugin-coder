#!/bin/bash

SCRIPT_PATH="${BASH_SOURCE[0]:-$0}"
WINDOWS_PS_HELPER="$(cd -- "$(dirname -- "$SCRIPT_PATH")" && pwd)/../Scripts/WindowsPowerShellSibling.sh"
if [[ ! -f "$WINDOWS_PS_HELPER" ]]; then
    echo "Error: Windows PowerShell helper not found at $WINDOWS_PS_HELPER"
    exit 1
fi

source "$WINDOWS_PS_HELPER"
moonbase_delegate_to_windows_powershell_if_needed "$SCRIPT_PATH" 0 "$@"

reset

cd "$(dirname "$0")/.."

RUN_VALIDATION_BENCHMARK=0

matrixJUCE="$1"
matrixCpp="$2"
runsOnGithub="$3"
diagnosticMode="${4:-}"

if [[ -n "$runsOnGithub" ]]; then
    export SEED="${SEED:-1337}"
fi

# on windows invoke vcvarsall.bat to setup the environment
if [[ -z "$runsOnGithub" ]]; then
    if [[ "$OSTYPE" == "cygwin" ]] || [[ "$OSTYPE" == "msys" ]]; then
        eval "$(powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./tests/msvc-init/vcvarsall.ps1 x64)"
    fi
fi


test_config_json="$(realpath ./tests/moonbase_api_config.json)"
if [ ! -f "$test_config_json" ]; then
    echo "Error: test config json not found at $test_config_json"
    exit 1
fi

if [[ -n "$runsOnGithub" ]] && moonbase_is_windows_shell; then
    powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -File ./tests/prebuild-parity.ps1 "$test_config_json" || exit $?
fi

"./PreBuild.sh" "$test_config_json" || exit $?

CXX_DEFINES="-DMB_CATCH2_TESTING -DJUCE_MODAL_LOOPS_PERMITTED=1"

SANITIZER_FLAGS=""
if [[ "${diagnosticMode}" == "sanitizers" ]]; then
    SANITIZER_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all"
    export ASAN_OPTIONS="${ASAN_OPTIONS:-abort_on_error=1:symbolize=1}"
    export UBSAN_OPTIONS="${UBSAN_OPTIONS:-print_stacktrace=1:halt_on_error=1}"
fi

if [ $RUN_VALIDATION_BENCHMARK -eq 1 ]; then
    CXX_DEFINES="${CXX_DEFINES} -DRUN_VALIDATION_BENCHMARK"
fi

# Conditionally add defines
if [ "${matrixJUCE}" == "JUCE7" ]; then
    CMAKE_JUCE="-DJUCE7=ON"
else
    CMAKE_JUCE=""
fi

if [ "${matrixCpp}" == "20" ]; then
    CMAKE_CPP="-DCPP20=ON"
else
    CMAKE_CPP=""
fi

CXX_FLAGS="${CXX_DEFINES}"
C_FLAGS=""
EXE_LINKER_FLAGS=""
SHARED_LINKER_FLAGS=""
if [[ -n "${SANITIZER_FLAGS}" ]]; then
    CXX_FLAGS="${CXX_FLAGS} ${SANITIZER_FLAGS}"
    C_FLAGS="${SANITIZER_FLAGS}"
    EXE_LINKER_FLAGS="${SANITIZER_FLAGS}"
    SHARED_LINKER_FLAGS="${SANITIZER_FLAGS}"
fi

CMAKE_ARGS="-B Builds -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER_LAUNCHER=sccache \
    -DCMAKE_CXX_COMPILER_LAUNCHER=sccache \
    -DCMAKE_OBJC_COMPILER_LAUNCHER=sccache \
    -DCMAKE_OBJCXX_COMPILER_LAUNCHER=sccache \
    -DCMAKE_C_FLAGS=\"${C_FLAGS}\" \
    -DCMAKE_CXX_FLAGS=\"${CXX_FLAGS}\" \
    -DCMAKE_EXE_LINKER_FLAGS=\"${EXE_LINKER_FLAGS}\" \
    -DCMAKE_SHARED_LINKER_FLAGS=\"${SHARED_LINKER_FLAGS}\" \
    ${CMAKE_JUCE} ${CMAKE_CPP}
"

if [[ -n "${NPROC:-}" ]]; then
    export numCpusCores="${NPROC}"
elif command -v nproc >/dev/null 2>&1; then
    export numCpusCores="$(nproc)"
elif command -v sysctl >/dev/null 2>&1; then
    export numCpusCores="$(sysctl -n hw.ncpu)"
else
    export numCpusCores=4
fi

ctestParallelLevel="${CTEST_PARALLEL_LEVEL:-1}"


eval cmake ${CMAKE_ARGS} . || exit 1
cmake --build Builds || exit 2

ctestExtraArgs="${CTEST_EXTRA_ARGS:-}"
TESTCOMMAND="ctest --test-dir Builds --parallel ${ctestParallelLevel} --output-on-failure ${ctestExtraArgs}"
if [ $RUN_VALIDATION_BENCHMARK -eq 1 ]; then
    TESTCOMMAND="${TESTCOMMAND} -V"
fi
# run test
numTests=1
for i in $(seq 1 $numTests); do
    echo "Running tests - iteration $i"
    eval ${TESTCOMMAND} || exit 3
done

if command -v sccache >/dev/null 2>&1; then
    sccache --show-stats || true
fi


exit 0
