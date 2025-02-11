#!/bin/bash

# Define color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No color

# Define test scripts and expected results
declare -A tests=(
    ["./curl-get-client-yaml-incorrect.sh"]="error: Internal Server Error"
    ["./curl-get-client-yaml.sh"]="---
name: John Doe
age: 30
email: johndoe@example.com
isMarried: false
children:
- name: Alice
  age: 5
- name: Bob
  age: 3
hobbies:
- reading
- cycling
- traveling"
    ["./curl-get-client-json-incorrect.sh"]='{"error": "Internal Server Error"}'
    ["./curl-get-client-json.sh"]='{
  "name": "John Doe",
  "age": 30,
  "email": "johndoe@example.com",
  "isMarried": false,
  "children": [
    {
      "name": "Alice",
      "age": 5
    },
    {
      "name": "Bob",
      "age": 3
    }
  ],
  "hobbies": [
    "reading",
    "cycling",
    "traveling"
  ]
}'
    ["./curl-get-client-no-header.sh"]='{"error": "Missing or Invalid Accept header"}'
    ["./curl-push-client-binary.sh"]=""
    ["./curl-push-client-json.sh"]=""
    ["./curl-push-client-yaml.sh"]=""
    ["./curl-get-filtered-client-json.sh"]='{
  "name": "John Doe",
  "age": 30,
  "email": "johndoe@example.com",
  "isMarried": false,
  "children": [
    {
      "name": "Alice",
      "age": 5
    },
    {
      "name": "Bob",
      "age": 3
    }
  ],
  "hobbies": [
    "reading",
    "cycling",
    "traveling"
  ],
  "pData": "category=X&price_lt=Y"
}'
  ["./curl-get-filtered-client-yaml.sh"]='---
name: John Doe
age: 30
email: johndoe@example.com
isMarried: false
children:
- name: Alice
  age: 5
- name: Bob
  age: 3
hobbies:
- reading
- cycling
- traveling
pData: category=X&price_lt=Y')

# Loop through tests and validate output
for script in "${!tests[@]}"; do
    echo "Running: $script"
    # Capture script output, removing leading/trailing whitespace
    output=$($script | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
    # Remove any color codes from output (if present)
    output=$(echo "$output" | sed -r 's/\x1B(\[[0-9;]*[JKmsu]|\(B)//g')

    # Get the expected result, also trimming any whitespace
    expected=$(echo "${tests[$script]}" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')

    # Compare outputs
    if [[ "$output" == "$expected" ]]; then
        echo -e "${GREEN}Test Passed: Output matches expected result.${NC}"
    else
        echo -e "${RED}Test Failed: Output does not match expected result.${NC}"
        echo -e "${YELLOW}Expected:${NC}"
        echo "$expected"
        echo -e "${YELLOW}Got:${NC}"
        echo "$output"
    fi
    echo "---------------------------------"
done
