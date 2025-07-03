#!/bin/bash

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m' # No Color

# Function to print colored output
print_colored() {
    echo -e "${1}${2}${NC}"
}

# Function to print section headers
print_header() {
    echo
    print_colored $CYAN "════════════════════════════════════════════════════════════════"
    print_colored $CYAN "  $1"
    print_colored $CYAN "════════════════════════════════════════════════════════════════"
    echo
}

# Function to print warnings in a nice format
format_warnings() {
    while IFS= read -r line; do
        if [[ $line == *"warning:"* ]]; then
            echo -e "${YELLOW}┌─ WARNING ─────────────────────────────────────────────────────────────────${NC}"
            echo -e "${YELLOW}│ ${YELLOW}$line${NC}"
            echo -e "${YELLOW}└───────────────────────────────────────────────────────────────────────────${NC}"
            echo
        elif [[ $line == *"error:"* ]]; then
            echo -e "${RED}┌─ ERROR ───────────────────────────────────────────────────────────────────${NC}"
            echo -e "${RED}│ ${RED}$line${NC}"
            echo -e "${RED}└───────────────────────────────────────────────────────────────────────────${NC}"
            echo
        elif [[ $line == *"note:"* ]]; then
            echo -e "${BLUE}┌─ NOTE ────────────────────────────────────────────────────────────────────${NC}"
            echo -e "${BLUE}│ ${BLUE}$line${NC}"
            echo -e "${BLUE}└───────────────────────────────────────────────────────────────────────────${NC}"
            echo
        else
            echo "$line"
        fi
    done
}

# Function to print ASCII art
print_ascii_art() {
    print_colored $MAGENTA "
███████╗ ██████╗  ██████╗██╗  ██╗███████╗
██╔════╝██╔═══██╗██╔════╝██║ ██╔╝██╔════╝
███████╗██║   ██║██║     █████╔╝ ███████╗
╚════██║██║   ██║██║     ██╔═██╗ ╚════██║
███████║╚██████╔╝╚██████╗██║  ██╗███████║
╚══════╝ ╚═════╝  ╚═════╝╚═╝  ╚═╝╚══════╝
"
    print_colored $GREEN "            Ready to Rock and Roll!"
    echo
}

# Function to show build statistics
show_build_stats() {
    local start_time=$1
    local end_time=$2
    local duration=$((end_time - start_time))
    
    echo
    print_colored $CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    print_colored $WHITE "BUILD STATISTICS"
    print_colored $CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    printf "Build time: %02d:%02d\n" $((duration / 60)) $((duration % 60))
    printf "Object files: %d\n" $(find src -name '*.o' | wc -l)
    printf "Executable: %s\n" "$(ls -lh main 2>/dev/null | awk '{print $5}' || echo 'N/A')"
    print_colored $CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

# Main build process
main() {
    local start_time=$(date +%s)
    
    print_colored $MAGENTA "Starting SOCKS5 Build Process..."
    
    # Clean phase
    print_header "CLEANING BUILD ARTIFACTS"
    if make clean 2>&1 | format_warnings; then
        print_colored $GREEN "Clean completed successfully"
    else
        print_colored $RED "Clean failed"
        exit 1
    fi
    
    # Build phase
    print_header "BUILDING PROJECT"
    print_colored $YELLOW "Compiling source files..."
    echo
    
    # Capture build output and format it
    if make all 2>&1 | tee >(format_warnings); then
        local end_time=$(date +%s)
        echo
        print_colored $GREEN "Build completed successfully!"
        
        # Show build statistics
        show_build_stats $start_time $end_time
        
        # Show ASCII art
        print_ascii_art
        
        print_colored $GREEN "Ready to execute: ./main"
        print_colored $CYAN "Run tests with: make test"
        
    else
        print_colored $RED "Build failed!"
        print_colored $YELLOW "Check the errors above and fix them before proceeding."
        exit 1
    fi
}

# Check if we're in the right directory
if [[ ! -f "Makefile" ]]; then
    print_colored $RED "Error: Makefile not found in current directory"
    print_colored $YELLOW "Please run this script from the project root directory"
    exit 1
fi

# Run the main function
main "$@"