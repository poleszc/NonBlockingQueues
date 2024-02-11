#!/bin/bash


# Check if build folder exists
if [ -d "build" ]; then
    # Remove build folder if it exists
    rm -rf build
fi

# Create build folder
mkdir build && cd build

# Check if the mkdir command succeeded
if [ $? -ne 0 ]; then
    echo "COMPILATION FAILED: Failed to create build directory."
    exit 1
fi
# Check if the script is run with DEBUG flag
if [ "$1" == "DEBUG" ]; then
    # Run cmake with DEBUG flag
    echo "debug"
    cmake -DDEBUG=ON .. &>/dev/null
else
    echo "normal"
    # Run cmake without DEBUG flag
    cmake .. &>/dev/null
fi

# Run cmake
#cmake ..

# Check if the cmake command succeeded
if [ $? -ne 0 ]; then
    echo "COMPILATION FAILED: CMake configuration failed."
    exit 1
fi

# Run make
#make &>/dev/null
make

# Check if the make command succeeded
if [ $? -ne 0 ]; then
    echo "COMPILATION FAILED: Make failed."
    exit 1
fi

# clear

echo "Compilation successful."

# Array to store the names of tests that passed and failed
passed_tests=()
failed_tests=()

# Run each test prefixed with "test_"
for test_file in test_*; do
    echo -e "Running: $test_file"
    if [ -z ${VALGRIND+x} ]; then
        ./"$test_file"
    else
        echo "VALGRING ON"
        valgrind -q --track-origins=yes --trace-children=yes --leak-check=full --show-leak-kinds=all ./"$test_file"
    fi

    if [ $? -eq 0 ]; then
        echo -e "\e[32m$test_file: OK\e[0m"
        passed_tests+=("$test_file: OK")
    else
        echo -e "\e[31m$test_file: ERROR\e[0m"
        failed_tests+=("$test_file: ERROR")
    fi
done

# Print the names of tests that passed with green color
echo -e "\n\e[32mTests that passed (${#passed_tests[@]}):\e[0m"
for passed_test in "${passed_tests[@]}"; do
    echo -e "\e[32m$passed_test\e[0m"
done

# Print the names of tests that failed with red color
echo -e "\n\e[31mTests that failed (${#failed_tests[@]}):\e[0m"
for failed_test in "${failed_tests[@]}"; do
    echo -e "\e[31m$failed_test\e[0m"
done