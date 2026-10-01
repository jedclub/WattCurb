#!/usr/bin/env bash
# WattCurb Build Artifact & Ephemeral Cache Cleaner
# Safely purges build directories, compiler outputs, and temporary logs.

set -euo pipefail

# 1. Resolve repository root safely
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="${SCRIPT_DIR}"

if [ ! -f "${REPO_ROOT}/CMakeLists.txt" ] || [ ! -f "${REPO_ROOT}/AGENTS.md" ]; then
    echo "[!] Error: Must be executed within the WattCurb repository tree." >&2
    exit 1
fi

cd "${REPO_ROOT}"

# Color formatting
if [ -t 1 ]; then
    BOLD="\033[1m"
    GREEN="\033[1;32m"
    YELLOW="\033[1;33m"
    RED="\033[1;31m"
    CYAN="\033[1;36m"
    RESET="\033[0m"
else
    BOLD=""
    GREEN=""
    YELLOW=""
    RED=""
    CYAN=""
    RESET=""
fi

# Options
DRY_RUN=0
CLEAN_ALL=0
KEEP_TMP=0
VERBOSE=0

show_help() {
    cat <<EOF
${BOLD}사용법:${RESET} $(basename "$0") [옵션]

${BOLD}설명:${RESET}
  WattCurb 프로젝트 내의 모든 컴파일 부산물, 빌드 디렉토리,
  출력 바이너리 및 임시 로그 파일들을 안전하게 정리(Clean-up)합니다.

${BOLD}옵션:${RESET}
  -n, --dry-run     실제 파일을 삭제하지 않고 정리될 대상과 예상 용량만 표시합니다.
  -a, --all         .cache 및 IDE 임시 캐시 디렉토리까지 모두 정리합니다.
      --keep-tmp    tmp/ 디렉토리 내 임시 로그 및 스크래치 파일을 유지합니다.
  -v, --verbose     삭제되는 각 디렉토리 및 파일 목록을 상세히 출력합니다.
  -h, --help        이 도움말 메시지를 표시합니다.

${BOLD}기본 정리 대상:${RESET}
  • build, build_*         (CMake/Ninja 빌드 트리 디렉토리)
  • output, output_*       (최종 릴리스 바이너리 및 스테이징 산출물)
  • tmp/*                  (테스트 로그, 하네스 로그 등 임시 파일)
  • *.gcda, *.gcno         (PGO / 커버리지 프로파일링 원시 데이터)
  • CMake 임시 잔여물      (CMakeCache.txt, CMakeFiles/, cmake_install.cmake 등)
EOF
}

# Parse CLI arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        -n|--dry-run)
            DRY_RUN=1
            shift
            ;;
        -a|--all)
            CLEAN_ALL=1
            shift
            ;;
        --keep-tmp)
            KEEP_TMP=1
            shift
            ;;
        -v|--verbose)
            VERBOSE=1
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            echo -e "${RED}[!] 알 수 없는 옵션: $1${RESET}" >&2
            show_help
            exit 1
            ;;
    esac
done

echo -e "${BOLD}${CYAN}===================================================================${RESET}"
echo -e "${BOLD}${CYAN}  ⚡ WattCurb 빌드 부산물 및 임시 파일 클린업 (Clean-up)           ${RESET}"
echo -e "${BOLD}${CYAN}===================================================================${RESET}"

if [ "${DRY_RUN}" -eq 1 ]; then
    echo -e "${YELLOW}ℹ [Dry-Run 모드] 실제 파일 삭제 없이 대상만 확인합니다.${RESET}\n"
fi

# List of tracked files in git to ensure we NEVER delete tracked files
GIT_TRACKED=$(git ls-files 2>/dev/null || true)
is_git_tracked() {
    local target="$1"
    # Remove leading ./ if present
    target="${target#./}"
    if echo "${GIT_TRACKED}" | grep -Fxq "${target}"; then
        return 0
    fi
    return 1
}

# Calculate total size before cleanup
TARGETS_DIR=()
TARGETS_FILE=()

# 1. Build and output directories in root
while IFS= read -r -d '' dir; do
    dir_name="$(basename "${dir}")"
    # Protect git tracked directories or system dirs
    if [ "${dir_name}" != ".git" ] && [ "${dir_name}" != "profiles" ] && [ "${dir_name}" != "scripts" ] && [ "${dir_name}" != "src" ] && [ "${dir_name}" != "docs" ] && [ "${dir_name}" != "tests" ] && [ "${dir_name}" != "desktop" ]; then
        TARGETS_DIR+=("${dir}")
    fi
done < <(find . -maxdepth 1 -type d \( -name "build" -o -name "build_*" -o -name "output" -o -name "output_*" -o -name "bin" \) -print0)

# 2. Ephemeral logs and scratch in tmp/
TMP_FILES=()
if [ "${KEEP_TMP}" -eq 0 ] && [ -d "tmp" ]; then
    while IFS= read -r -d '' item; do
        if [ "${item}" != "./tmp" ]; then
            TMP_FILES+=("${item}")
        fi
    done < <(find tmp -mindepth 1 -maxdepth 1 -print0 2>/dev/null || true)
fi

# 3. Cache directory if --all
if [ "${CLEAN_ALL}" -eq 1 ] && [ -d ".cache" ]; then
    TARGETS_DIR+=("./.cache")
fi

# 4. Loose CMake / compiler artifacts in tree
while IFS= read -r -d '' file; do
    if ! is_git_tracked "${file}"; then
        TARGETS_FILE+=("${file}")
    fi
done < <(find . -not -path "*/.*" -not -path "./build*" -not -path "./output*" -not -path "./tmp*" \
    \( -name "CMakeCache.txt" \
    -o -name "CMakeFiles" \
    -o -name "cmake_install.cmake" \
    -o -name "CTestTestfile.cmake" \
    -o -name "compile_commands.json" \
    -o -name "*.o" \
    -o -name "*.obj" \
    -o -name "*.gcda" \
    -o -name "*.gcno" \
    -o -name "*.so" \
    -o -name "*.a" \
    -o -name "*.dylib" \
    -o -name "*.out" \) -print0 2>/dev/null || true)

TOTAL_COUNT=$(( ${#TARGETS_DIR[@]} + ${#TARGETS_FILE[@]} + ${#TMP_FILES[@]} ))

if [ "${TOTAL_COUNT}" -eq 0 ]; then
    echo -e "${GREEN}✓ 정리할 빌드 부산물이 없습니다. 프로젝트가 이미 깨끗합니다.${RESET}"
    exit 0
fi

# Measure disk usage of targets
calc_size() {
    local paths=("$@")
    if [ ${#paths[@]} -eq 0 ]; then
        echo "0"
        return
    fi
    du -csk "${paths[@]}" 2>/dev/null | tail -n 1 | awk '{print $1}'
}

ALL_PATHS=()
for d in "${TARGETS_DIR[@]}"; do ALL_PATHS+=("${d}"); done
for f in "${TARGETS_FILE[@]}"; do ALL_PATHS+=("${f}"); done
for t in "${TMP_FILES[@]}"; do ALL_PATHS+=("${t}"); done

ESTIMATED_KB=$(calc_size "${ALL_PATHS[@]}")
ESTIMATED_MB=$(awk "BEGIN {printf \"%.2f\", ${ESTIMATED_KB}/1024}")

echo -e "발견된 정리 대상: ${BOLD}${TOTAL_COUNT}개 항목${RESET} (약 ${BOLD}${ESTIMATED_MB} MB${RESET})"
echo ""

# Process directories
if [ ${#TARGETS_DIR[@]} -gt 0 ]; then
    echo -e "${BOLD}1. 빌드 및 산출물 디렉토리 (${#TARGETS_DIR[@]}개):${RESET}"
    for dir in "${TARGETS_DIR[@]}"; do
        sz=$(du -sh "${dir}" 2>/dev/null | cut -f1 || echo "0")
        echo -e "  • ${CYAN}${dir}/${RESET} (${sz})"
        if [ "${DRY_RUN}" -eq 0 ]; then
            rm -rf "${dir}"
        fi
    done
    echo ""
fi

# Process tmp contents
if [ ${#TMP_FILES[@]} -gt 0 ]; then
    echo -e "${BOLD}2. 임시 로그 및 스크래치 파일 (tmp/*, ${#TMP_FILES[@]}개):${RESET}"
    if [ "${VERBOSE}" -eq 1 ]; then
        for item in "${TMP_FILES[@]}"; do
            echo -e "  • ${item}"
        done
    else
        echo -e "  • tmp/ 디렉토리 내 로그 및 임시 파일 ${#TMP_FILES[@]}개 일괄 정리"
    fi
    if [ "${DRY_RUN}" -eq 0 ]; then
        rm -rf tmp/* tmp/.* 2>/dev/null || true
        # Ensure tmp directory still exists for future harness runs
        mkdir -p tmp
    fi
    echo ""
fi

# Process loose artifacts
if [ ${#TARGETS_FILE[@]} -gt 0 ]; then
    echo -e "${BOLD}3. 기타 CMake / 컴파일 잔여물 (${#TARGETS_FILE[@]}개):${RESET}"
    for file in "${TARGETS_FILE[@]}"; do
        echo -e "  • ${file}"
        if [ "${DRY_RUN}" -eq 0 ]; then
            rm -rf "${file}"
        fi
    done
    echo ""
fi

if [ "${DRY_RUN}" -eq 1 ]; then
    echo -e "${YELLOW}ℹ [Dry-Run 완료] 삭제된 파일은 없습니다. 실제 삭제하려면 옵션 없이 실행하십시오.${RESET}"
else
    echo -e "${GREEN}✓ 클린업 완료! 총 약 ${BOLD}${ESTIMATED_MB} MB${RESET}${GREEN}의 디스크 공간이 확보되었습니다.${RESET}"
fi
