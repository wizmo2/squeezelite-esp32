#!/usr/bin/env bash
set -e

. $IDF_PATH/export.sh
echo "npm version is $(npm --version)"
echo "node version is $(node --version)"
echo "To build the web application, run:"
echo "pushd components/wifi-manager/webapp/ && npm rebuild node-sass && npm run-script build && popd"
exec "$@"
