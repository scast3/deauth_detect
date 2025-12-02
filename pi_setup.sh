#!/bin/bash

set -e

sudo apt update

# DuckDB installation
curl https://install.duckdb.org | sh

# DuckDB C++ API
wget https://install.duckdb.org/v1.4.2/libduckdb-linux-arm64.zip
unzip libduckdb-linux-arm64.zip
mv duckdb.* /usr/local/include/
mv libduckdb* /usr/local/lib/
rm libduckdb-linux-arm64.zip
