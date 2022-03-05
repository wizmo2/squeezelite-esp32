#!/usr/bin/env bash
set -e

. $IDF_PATH/export.sh
echo "npm version is $(npm --version)"
echo "node version is $(node --version)"
echo "To build the web application, run:"
echo "pushd components/wifi-manager/webapp/ && npm rebuild node-sass && npm run-script build && popd"
echo ""
echo "To run size statistics, run:"
echo "puncover/runner.py --gcc_tools_base $GCC_TOOLS_BASE --elf ./build/recovery.elf --build_dir build --src_root ."
echo "" 
exec "$@"
