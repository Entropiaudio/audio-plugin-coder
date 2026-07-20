#!/bin/bash

SCRIPT_PATH="${BASH_SOURCE[0]:-$0}"
WINDOWS_PS_HELPER="$(cd -- "$(dirname -- "$SCRIPT_PATH")" && pwd)/WindowsPowerShellSibling.sh"
if [[ ! -f "$WINDOWS_PS_HELPER" ]]; then
    echo "Error: Windows PowerShell helper not found at $WINDOWS_PS_HELPER"
    exit 1
fi

source "$WINDOWS_PS_HELPER"
moonbase_delegate_to_windows_powershell_if_needed "$SCRIPT_PATH" 1 "$@"

PREBUILD_CACHE_RESULT_CONTINUE=0
PREBUILD_CACHE_RESULT_SKIP=10

PREBUILD_CACHE_ENABLED=0
PREBUILD_CACHE_DIR=""
PREBUILD_CACHE_KEY=""
PREBUILD_FINGERPRINT_FILE=""
PREBUILD_FINGERPRINT_CURRENT=""

function normalize_build_config_name () {
    local config_name="$1"
    config_name="$(printf '%s' "${config_name}" | tr '[:upper:]' '[:lower:]')"
    config_name="${config_name//[[:space:]]/}"
    printf '%s' "${config_name}"
}

function first_nonempty_env_value () {
    local var_name
    for var_name in "$@"; do
        if [[ -n "${!var_name:-}" ]]; then
            printf '%s' "${!var_name}"
            return 0
        fi
    done
    return 1
}

function should_enable_prebuild_cache () {
    local detected_config=""

    if ! detected_config="$(first_nonempty_env_value \
        BUILD_CONFIG \
        CMAKE_BUILD_TYPE \
        CONFIGURATION \
        Configuration \
        CONFIG \
        Config \
        BUILD_TYPE \
        CMAKE_CONFIG_TYPE \
        CMAKE_CONFIGURATION \
    )"; then
        return 1
    fi

    detected_config="$(normalize_build_config_name "${detected_config}")"
    [[ "${detected_config}" == "debug" ]]
}

function hash_file_sha1 () {
    local file="$1"

    if command -v shasum >/dev/null 2>&1; then
        shasum "$file" | awk '{ print $1 }'
    elif command -v sha1sum >/dev/null 2>&1; then
        sha1sum "$file" | awk '{ print $1 }'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha1 "$file" | awk '{ print $NF }'
    else
        return 1
    fi
}

function hash_stream_sha1 () {
    if command -v shasum >/dev/null 2>&1; then
        shasum | awk '{ print $1 }'
    elif command -v sha1sum >/dev/null 2>&1; then
        sha1sum | awk '{ print $1 }'
    elif command -v openssl >/dev/null 2>&1; then
        openssl dgst -sha1 | awk '{ print $NF }'
    else
        return 1
    fi
}

function hash_text_sha1 () {
    local text="$1"
    printf '%s' "$text" | hash_stream_sha1
}

function canonicalize_path () {
    local path="$1"

    if [[ -d "${path}" ]]; then
        (cd "${path}" && pwd -P)
        return $?
    fi

    local path_dir
    local path_base
    path_dir="$(dirname "${path}")"
    path_base="$(basename "${path}")"
    (cd "${path_dir}" && printf '%s/%s\n' "$(pwd -P)" "${path_base}")
}

function compute_prebuild_fingerprint () {
    local module_dir="$1"
    local config_file="$2"
    local tracked_files=(
        "${config_file}"
        "${module_dir}/PreBuild.sh"
        "${module_dir}/PreBuild.ps1"
        "${module_dir}/Scripts/PreBuildCacheHelpers.sh"
        "${module_dir}/Scripts/PreBuildCacheHelpers.ps1"
        "${module_dir}/Assets/Build.sh"
        "${module_dir}/Assets/Build.ps1"
        "${module_dir}/KeyIntegrity/IntegrityCheck.sh"
        "${module_dir}/KeyIntegrity/IntegrityCheck.ps1"
        "${module_dir}/GenerateConfigDetails/GenerateConfigDetails.sh"
        "${module_dir}/GenerateConfigDetails/GenerateConfigDetails.ps1"
        "${module_dir}/GenerateConfigDetails/GenerateConfigDetails_macOS"
        "${module_dir}/GenerateConfigDetails/GenerateConfigDetails_linux"
        "${module_dir}/GenerateConfigDetails/GenerateConfigDetails_windows.exe"
    )
    local tracked_file

    {
        for tracked_file in "${tracked_files[@]}"; do
            if [[ -f "${tracked_file}" ]]; then
                echo "$(hash_file_sha1 "${tracked_file}") ${tracked_file}"
            fi
        done

        while IFS= read -r file; do
            echo "$(hash_file_sha1 "${file}") ${file}"
        done < <(
            {
                find "${module_dir}/Assets/Source" "${module_dir}/KeyIntegrity" -type f
                find "${module_dir}/GenerateConfigDetails" -type f
                find "${module_dir}/Assets" -maxdepth 1 -type f -name "binaryBuilder*"
            } | LC_ALL=C sort
        )
    } | hash_stream_sha1
}

function prebuild_outputs_exist () {
    local module_dir="$1"
    local outputs=(
        "${module_dir}/Assets/MoonbaseBinary.h"
        "${module_dir}/Assets/BinaryIncludes.cpp"
        "${module_dir}/Source/Implementations/IntegrityCheck.generated.h"
        "${module_dir}/Source/ConfigDetails.generated.h"
    )

    local output
    for output in "${outputs[@]}"; do
        if [[ ! -f "${output}" ]]; then
            return 1
        fi
    done

    if ! ls "${module_dir}"/Assets/MoonbaseBinary*.cpp >/dev/null 2>&1; then
        return 1
    fi

    return 0
}

function prebuild_cache_prepare () {
    local module_dir="$1"
    local config_file="$2"
    local canonical_config_file=""
    local prebuild_fingerprint_prev=""

    canonical_config_file="$(canonicalize_path "${config_file}")" || return $?

    PREBUILD_CACHE_ENABLED=0
    if should_enable_prebuild_cache; then
        PREBUILD_CACHE_ENABLED=1
    fi
    PREBUILD_CACHE_DIR="${module_dir}/.moonbase-prebuild-cache"
    PREBUILD_CACHE_KEY=""
    PREBUILD_FINGERPRINT_FILE=""
    PREBUILD_FINGERPRINT_CURRENT=""

    if [[ "${PREBUILD_CACHE_ENABLED}" != "1" ]]; then
        return "${PREBUILD_CACHE_RESULT_CONTINUE}"
    fi

    PREBUILD_CACHE_KEY="$(hash_text_sha1 "${module_dir}")" || return $?
    PREBUILD_FINGERPRINT_FILE="${PREBUILD_CACHE_DIR}/moonbase_prebuild_${PREBUILD_CACHE_KEY}.fingerprint"
    mkdir -p "${PREBUILD_CACHE_DIR}"

    PREBUILD_FINGERPRINT_CURRENT="$(compute_prebuild_fingerprint "${module_dir}" "${canonical_config_file}")" || return $?

    if [[ -f "${PREBUILD_FINGERPRINT_FILE}" ]]; then
        prebuild_fingerprint_prev=$(<"${PREBUILD_FINGERPRINT_FILE}")
    fi

    if [[ "${PREBUILD_FINGERPRINT_CURRENT}" == "${prebuild_fingerprint_prev}" ]] && prebuild_outputs_exist "${module_dir}"; then
        return "${PREBUILD_CACHE_RESULT_SKIP}"
    fi

    return "${PREBUILD_CACHE_RESULT_CONTINUE}"
}

function prebuild_cache_finalize () {
    if [[ "${PREBUILD_CACHE_ENABLED}" != "1" ]]; then
        return 0
    fi

    if [[ -z "${PREBUILD_FINGERPRINT_FILE}" || -z "${PREBUILD_FINGERPRINT_CURRENT}" ]]; then
        return 1
    fi

    echo "${PREBUILD_FINGERPRINT_CURRENT}" > "${PREBUILD_FINGERPRINT_FILE}"
    return 0
}

function run_prebuild_step () {
    local step_key="$1"
    local module_dir="$2"
    shift 2

    local step_log
    step_log="$(mktemp)"

    "$@" >"${step_log}" 2>&1
    local step_result=$?

    if [[ ${step_result} -ne 0 ]]; then
        echo "Moonbase PreBuild [${step_key}]: failed."
        if [[ -s "${step_log}" ]]; then
            cat "${step_log}"
        fi
        rm -f "${step_log}"
        return ${step_result}
    fi

    local step_summary="completed"
    case "${step_key}" in
        assets)
            local moonbase_cpp_files=("${module_dir}"/Assets/MoonbaseBinary*.cpp)
            local moonbase_cpp_count=0
            if [[ -e "${moonbase_cpp_files[0]}" ]]; then
                moonbase_cpp_count="${#moonbase_cpp_files[@]}"
            fi
            step_summary="generated Assets/BinaryIncludes.cpp and ${moonbase_cpp_count} MoonbaseBinary*.cpp file(s)"
            ;;
        integrity)
            local checks_added=""
            local line=""
            while IFS= read -r line; do
                if [[ "${line}" =~ added[[:space:]]+([0-9]+)[[:space:]]+checks ]]; then
                    checks_added="${BASH_REMATCH[1]}"
                fi
            done < "${step_log}"

            if [[ -n "${checks_added}" ]]; then
                step_summary="added ${checks_added} checks"
            else
                step_summary="updated Source/Implementations/IntegrityCheck.generated.h"
            fi
            ;;
        config)
            local wrote_path=""
            local module_prefix="${module_dir}/"
            local line=""
            while IFS= read -r line; do
                if [[ "${line}" == Wrote:* ]]; then
                    wrote_path="${line#Wrote: }"
                fi
            done < "${step_log}"

            if [[ -n "${wrote_path}" ]]; then
                if [[ "${wrote_path}" == "${module_prefix}"* ]]; then
                    wrote_path="${wrote_path#${module_prefix}}"
                fi
                step_summary="wrote ${wrote_path}"
            else
                step_summary="wrote Source/ConfigDetails.generated.h"
            fi
            ;;
    esac

    rm -f "${step_log}"
    echo "Moonbase PreBuild [${step_key}]: ran (${step_summary})."
    return 0
}

function run_moonbase_prebuild_cached () {
    local module_dir="$1"
    local config_file="$2"
    local cache_prepare_result=0

    prebuild_cache_prepare "${module_dir}" "${config_file}"
    cache_prepare_result=$?

    if [[ "${cache_prepare_result}" -eq "${PREBUILD_CACHE_RESULT_SKIP}" ]]; then
        echo "Moonbase PreBuild [assets]: skipped (cache hit)."
        echo "Moonbase PreBuild [integrity]: skipped (cache hit)."
        echo "Moonbase PreBuild [config]: skipped (cache hit)."
        return 0
    fi

    if [[ "${cache_prepare_result}" -ne "${PREBUILD_CACHE_RESULT_CONTINUE}" ]]; then
        return "${cache_prepare_result}"
    fi

    run_prebuild_step "assets" "${module_dir}" "${module_dir}/Assets/Build.sh" || return $?
    run_prebuild_step "integrity" "${module_dir}" "${module_dir}/KeyIntegrity/IntegrityCheck.sh" "${config_file}" || return $?
    run_prebuild_step "config" "${module_dir}" "${module_dir}/GenerateConfigDetails/GenerateConfigDetails.sh" "${config_file}" "${module_dir}/Source/ConfigDetails.generated.h" || return $?

    prebuild_cache_finalize || return $?
    return 0
}
