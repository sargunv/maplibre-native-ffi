# Build and development tasks for maplibre-native-ffi

# List available recipes
default:
    @just --list

# Configure CMake build in Debug mode
configure:
    cmake -B build -DCMAKE_BUILD_TYPE=Debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Configure CMake build in Release mode
configure-release:
    cmake -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Build the project (after configure)
build: configure
    cmake --build build

# Build the project in release mode
build-release: configure-release
    cmake --build build

# Clean build directory
clean:
    rm -rf build/*

# Format code using clang-format
format:
    find src include test -type f \( -name "*.cpp" -name "*.mm" -o -name "*.h" \) -exec clang-format -i {} +

# Run clang-tidy checks
lint: configure
    find src include test -type f \( -name "*.cpp" -name "*.mm" -o -name "*.h" \) -exec clang-tidy -p=build {} +

# Run clang-tidy and fix issues
tidy: configure
    find src include test -type f \( -name "*.cpp" -name "*.mm" -o -name "*.h" \) -exec clang-tidy -fix -p=build {} +

# Format and lint
check: format lint

# Clean build and rebuild
rebuild: clean build

# Run tests
test: configure
    cd build && ./tests
