#!/bin/bash
# Setup local caching for faster PebbleOS builds
# Source this file in your shell: source setup-local-cache.sh

# Detect OS
if [[ "$OSTYPE" == "darwin"* ]]; then
    # macOS
    CACHE_BASE="${TMPDIR}/pebbleos-cache"
else
    # Linux and others
    CACHE_BASE="${TMPDIR:-/tmp}/pebbleos-cache"
fi

# Create cache directories
mkdir -p "${CACHE_BASE}/pip"
mkdir -p "${CACHE_BASE}/ccache"

# Export ccache configuration
export CCACHE_DIR="${CACHE_BASE}/ccache"
export CCACHE_MAXSIZE="10G"
export CCACHE_COMPRESS="1"
export CCACHE_COMPRESSLEVEL="6"
export CCACHE_SLOPPINESS="file_macro,time_macros"

# Export pip cache configuration
export PIP_CACHE_DIR="${CACHE_BASE}/pip"

# Add ccache to PATH if it exists
if command -v ccache &> /dev/null; then
    # Check if ccache is already in PATH
    if ! command -v ccache &> /dev/null || ! which arm-none-eabi-gcc | grep -q ccache; then
        # Create symlinks for common compilers in cache directory
        mkdir -p "${CACHE_BASE}/compiler-links"
        for compiler in arm-none-eabi-gcc arm-none-eabi-g++ arm-none-eabi-cpp arm-none-eabi-ar; do
            if command -v $compiler &> /dev/null && [[ ! -L "${CACHE_BASE}/compiler-links/$compiler" ]]; then
                ln -sf $(which $compiler) "${CACHE_BASE}/compiler-links/$compiler"
            fi
        done
        export PATH="${CACHE_BASE}/compiler-links:${PATH}"
    fi
fi

echo "Local caching configured:"
echo "  CCACHE_DIR: ${CCACHE_DIR}"
echo "  PIP_CACHE_DIR: ${PIP_CACHE_DIR}"
echo ""
echo "To make this persistent, add this line to your ~/.bashrc or ~/.zshrc:"
echo "  source $(pwd)/setup-local-cache.sh"
