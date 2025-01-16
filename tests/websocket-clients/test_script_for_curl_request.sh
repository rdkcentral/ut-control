#!/bin/bash

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No color

# Define test scripts and expected results
declare -A tests=(
    ["./curl-get-client-yaml-incorrect.sh"]="error: Internal Server Error"
    ["./curl-get-client-yaml.sh"]="---
test:
  yamlData: somevalue
  x: 1
  on: true
test2:
  yamlData1: somevalue1
  y: 2
  off: false"
    ["./curl-get-client-json-incorrect.sh"]='{"error": "Internal Server Error"}'
    ["./curl-get-client-json.sh"]='{
  "test": {
    "jsonData": "somevalue",
    "x": 1,
    "on": true
  },
  "test2": {
    "jsonData1": "somevalue1",
    "y": 2,
    "off": false
  }
}'
    ["./curl-get-client-no-header.sh"]='{"error": "Unsupported Accept header or invalid type"}'
    ["./curl-push-client-binary.sh"]=""
    ["./curl-push-client-json.sh"]=""
    ["./curl-push-client-yaml.sh"]=""
)

# Loop through tests and validate output
for script in "${!tests[@]}"; do
    echo "Running: $script"
    output=$($script)
    expected="${tests[$script]}"

    if [[ "$output" == "$expected" ]]; then
        echo -e "${GREEN}Test Passed: Output matches expected result.${NC}"
    else
        echo -e "${RED}Test Failed: Output does not match expected result.${NC}"
        echo -e "${YELLOW}Expected:${NC}"
        echo "$expected"
        echo "${YELLOW}Got:${NC}"
        echo "$output"
    fi
    echo "---------------------------------"
done
