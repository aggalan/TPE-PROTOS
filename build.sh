#!/bin/bash

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
MAGENTA='\033[0;35m'
CYAN='\033[0;36m'
WHITE='\033[1;37m'
NC='\033[0m'

BRIGHT_RED='\033[1;31m'
BRIGHT_GREEN='\033[1;32m'
BRIGHT_YELLOW='\033[1;33m'
BRIGHT_BLUE='\033[1;34m'
BRIGHT_MAGENTA='\033[1;35m'
BRIGHT_CYAN='\033[1;36m'
ORANGE='\033[0;33m'
PURPLE='\033[0;35m'
PINK='\033[1;95m'
LIME='\033[1;92m'
TEAL='\033[0;96m'
LAVENDER='\033[0;94m'

BG_RED='\033[41m'
BG_GREEN='\033[42m'
BG_YELLOW='\033[43m'
BG_BLUE='\033[44m'
BG_MAGENTA='\033[45m'
BG_CYAN='\033[46m'

BOLD='\033[1m'
DIM='\033[2m'
UNDERLINE='\033[4m'
BLINK='\033[5m'
REVERSE='\033[7m'

# Build mode variables
ADMIN_MODE=false
BUILD_TARGET="all"
EXECUTABLE_NAME="socks5d"
MODE_TEXT="SOCKS5 Server"

print_colored() {
    echo -e "${1}${2}${NC}"
}

print_header() {
    echo
    print_colored $BRIGHT_CYAN "════════════════════════════════════════════════════════════════"
    print_colored $BRIGHT_YELLOW " $1 "
    print_colored $BRIGHT_CYAN "════════════════════════════════════════════════════════════════"
    echo
}

format_warnings() {
  local prefix code_line
  while IFS= read -r line; do
    if [[ $line =~ ^([[:space:]]*[0-9]+[[:space:]]*\|)(.*)$ ]]; then
      prefix="${BASH_REMATCH[1]}"
      code_line="${BASH_REMATCH[2]}"
      if [[ $prefix =~ ^([[:space:]]*)([0-9]+)([[:space:]]*\|)$ ]]; then
        local spaces="${BASH_REMATCH[1]}"
        local line_num="${BASH_REMATCH[2]}"
        local pipe="${BASH_REMATCH[3]}"
        prefix="${spaces}${BRIGHT_BLUE}${line_num}${TEAL}${pipe}${NC}"
      fi
      continue
    fi

    if [[ $line =~ ^[[:space:]]*\|([[:space:]]*)(\^[~^]*|~+)(.*)$ ]]; then
      local indent=${#BASH_REMATCH[1]}
      local caret_part="${BASH_REMATCH[2]}" 
      local clen=${#caret_part}           
      
      local before=${code_line:0:indent}
      local frag=${code_line:indent:clen}
      local after=${code_line:indent+clen}

      echo -e "${prefix}${before}${BG_RED}${BRIGHT_YELLOW}${frag}${NC}${after}"
      echo -e "${BRIGHT_RED}${BLINK}${line}${NC}"

      prefix=""; code_line=""
      continue
    fi

    if [[ $line =~ ^(src/[^:]+):([0-9]+):([0-9]+):[[:space:]]*(warning|error|note|fatal error):[[:space:]]*(.*)$ ]]; then
      local file="${BASH_REMATCH[1]}"
      local line_no="${BASH_REMATCH[2]}"
      local col_no="${BASH_REMATCH[3]}"
      local type="${BASH_REMATCH[4]}"
      local message="${BASH_REMATCH[5]}"
      
      echo -ne "${BRIGHT_CYAN}${file}${NC}:${BRIGHT_MAGENTA}${line_no}${NC}:${BRIGHT_MAGENTA}${col_no}${NC}: "
      
      case "$type" in
        "warning") echo -e "${BG_YELLOW}${BOLD} WARNING ${NC} ${ORANGE}${message}${NC}" ;;
        "error") echo -e "${BG_RED}${BRIGHT_YELLOW}${BOLD} ERROR ${NC} ${BRIGHT_RED}${message}${NC}" ;;
        "fatal error") echo -e "${BG_RED}${BRIGHT_WHITE}${BOLD} FATAL ERROR ${NC} ${BRIGHT_RED}${UNDERLINE}${message}${NC}" ;;
        "note") echo -e "${BG_BLUE}${BRIGHT_WHITE}${BOLD} NOTE ${NC} ${BRIGHT_BLUE}${message}${NC}" ;;
      esac
      continue
    fi

    if [[ $line =~ ^(In function|At top level|In file included from) ]]; then
      echo -e "${PURPLE}${BOLD}${line}${NC}"
      continue
    fi

    echo "$line"
  done
}

print_ascii_art() {
    if [[ "$ADMIN_MODE" == true ]]; then
        print_colored $BRIGHT_MAGENTA "
 ${BRIGHT_CYAN}███████╗${BRIGHT_MAGENTA} ██████╗ ${BRIGHT_YELLOW} ██████╗${BRIGHT_GREEN}██╗  ██╗${BRIGHT_BLUE}███████╗
 ${BRIGHT_CYAN}██╔════╝${BRIGHT_MAGENTA}██╔═══██╗${BRIGHT_YELLOW}██╔════╝${BRIGHT_GREEN}██║ ██╔╝${BRIGHT_BLUE}██╔════╝
 ${BRIGHT_CYAN}███████╗${BRIGHT_MAGENTA}██║   ██║${BRIGHT_YELLOW}██║     ${BRIGHT_GREEN}█████╔╝ ${BRIGHT_BLUE}███████╗
 ${BRIGHT_CYAN}╚════██║${BRIGHT_MAGENTA}██║   ██║${BRIGHT_YELLOW}██║     ${BRIGHT_GREEN}██╔═██╗ ${BRIGHT_BLUE}╚════██║
 ${BRIGHT_CYAN}███████║${BRIGHT_MAGENTA}╚██████╔╝${BRIGHT_YELLOW}╚██████╗${BRIGHT_GREEN}██║  ██╗${BRIGHT_BLUE}███████║
 ${BRIGHT_CYAN}╚══════╝${BRIGHT_MAGENTA} ╚═════╝ ${BRIGHT_YELLOW} ╚═════╝${BRIGHT_GREEN}╚═╝  ╚═╝${BRIGHT_BLUE}╚══════╝
"
        print_colored $ORANGE "           Admin Client Ready!"
    else
        print_colored $BRIGHT_MAGENTA "
 ${BRIGHT_CYAN}███████╗${BRIGHT_MAGENTA} ██████╗ ${BRIGHT_YELLOW} ██████╗${BRIGHT_GREEN}██╗  ██╗${BRIGHT_BLUE}███████╗
 ${BRIGHT_CYAN}██╔════╝${BRIGHT_MAGENTA}██╔═══██╗${BRIGHT_YELLOW}██╔════╝${BRIGHT_GREEN}██║ ██╔╝${BRIGHT_BLUE}██╔════╝
 ${BRIGHT_CYAN}███████╗${BRIGHT_MAGENTA}██║   ██║${BRIGHT_YELLOW}██║     ${BRIGHT_GREEN}█████╔╝ ${BRIGHT_BLUE}███████╗
 ${BRIGHT_CYAN}╚════██║${BRIGHT_MAGENTA}██║   ██║${BRIGHT_YELLOW}██║     ${BRIGHT_GREEN}██╔═██╗ ${BRIGHT_BLUE}╚════██║
 ${BRIGHT_CYAN}███████║${BRIGHT_MAGENTA}╚██████╔╝${BRIGHT_YELLOW}╚██████╗${BRIGHT_GREEN}██║  ██╗${BRIGHT_BLUE}███████║
 ${BRIGHT_CYAN}╚══════╝${BRIGHT_MAGENTA} ╚═════╝ ${BRIGHT_YELLOW} ╚═════╝${BRIGHT_GREEN}╚═╝  ╚═╝${BRIGHT_BLUE}╚══════╝
"
        print_colored $LIME "            Ready to Rock and Roll!"
    fi
    echo
}

show_build_stats() {
    local start_time=$1
    local end_time=$2
    local duration=$((end_time - start_time))
    
    echo
    print_colored $BRIGHT_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    print_colored $BG_BLUE$BRIGHT_WHITE$BOLD "                     BUILD STATISTICS                     "
    print_colored $BRIGHT_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    printf "${BRIGHT_YELLOW} Build time: ${BRIGHT_GREEN}%02d:%02d${NC}\n" $((duration / 60)) $((duration % 60))
    printf "${BRIGHT_YELLOW} Object files: ${BRIGHT_GREEN}%d${NC}\n" $(find src -name '*.o' 2>/dev/null | wc -l)
    printf "${BRIGHT_YELLOW} Executable: ${BRIGHT_GREEN}%s${NC}\n" "$(ls -lh $EXECUTABLE_NAME 2>/dev/null | awk '{print $5}' || echo 'N/A')"
    printf "${BRIGHT_YELLOW} Build mode: ${BRIGHT_GREEN}%s${NC}\n" "$MODE_TEXT"
    print_colored $BRIGHT_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

show_usage() {
    print_colored $BRIGHT_CYAN "Usage: $0 [OPTIONS]"
    print_colored $BRIGHT_YELLOW "Options:"
    print_colored $BRIGHT_WHITE "  -a, --admin    Build admin client instead of server"
    print_colored $BRIGHT_WHITE "  -h, --help     Show this help message"
    echo
}

parse_arguments() {
    while [[ $# -gt 0 ]]; do
        case $1 in
            -a|--admin)
                ADMIN_MODE=true
                BUILD_TARGET="admin_client"
                EXECUTABLE_NAME="admin_client"
                MODE_TEXT="Admin Client"
                shift
                ;;
            -h|--help)
                show_usage
                exit 0
                ;;
            *)
                print_colored $BG_RED$BRIGHT_WHITE$BOLD " Error: Unknown option '$1' "
                show_usage
                exit 1
                ;;
        esac
    done
}

main() {
    local start_time=$(date +%s)
    
    if [[ "$ADMIN_MODE" == true ]]; then
        print_colored $BRIGHT_MAGENTA "${BOLD}Starting SOCKS5 Admin Client Build Process...${NC}"
        print_colored $ORANGE "${BOLD}Building in Administrative Mode${NC}"
    else
        print_colored $BRIGHT_MAGENTA "${BOLD}Starting SOCKS5 Build Process...${NC}"
    fi
    
    print_header "CLEANING BUILD ARTIFACTS"

    clean_output=$(mktemp)
    if make clean >"$clean_output" 2>&1; then
        cat "$clean_output" | format_warnings
        print_colored $BRIGHT_GREEN "${BOLD}Clean completed successfully!${NC}"
    else
        cat "$clean_output" | format_warnings
        print_colored $BG_RED$BRIGHT_WHITE$BOLD " Clean failed! "
        rm -f "$clean_output"
        exit 1
    fi
    rm -f "$clean_output"
    
    print_header "BUILDING PROJECT"
    if [[ "$ADMIN_MODE" == true ]]; then
        print_colored $BRIGHT_YELLOW "${BOLD}Compiling admin client...${NC}"
    else
        print_colored $BRIGHT_YELLOW "${BOLD}Compiling source files...${NC}"
    fi
    echo
    
    if make $BUILD_TARGET 2>&1 | format_warnings; then
        local end_time=$(date +%s)
        echo
        print_colored $BG_GREEN$BRIGHT_WHITE$BOLD " Build completed successfully! "
        
        show_build_stats $start_time $end_time
        
        print_ascii_art
        
        if [[ "$ADMIN_MODE" == true ]]; then
            print_colored $BRIGHT_GREEN "${BOLD}Ready to execute: ${BRIGHT_CYAN}./$EXECUTABLE_NAME${NC}"
            print_colored $BRIGHT_BLUE "${BOLD}Admin client built successfully!${NC}"
        else
            print_colored $BRIGHT_GREEN "${BOLD}Ready to execute: ${BRIGHT_CYAN}./$EXECUTABLE_NAME${NC}"
            print_colored $BRIGHT_BLUE "${BOLD}Run tests with: ${BRIGHT_CYAN}./test.sh${NC}"
        fi
        
    else
        print_colored $BG_RED$BRIGHT_WHITE$BOLD " Build failed! "
        print_colored $BRIGHT_YELLOW "${BOLD}Check the errors above and fix them before proceeding.${NC}"
        exit 1
    fi
}

# Check if Makefile exists
if [[ ! -f "Makefile" ]]; then
    print_colored $BG_RED$BRIGHT_WHITE$BOLD " Error: Makefile not found in current directory "
    print_colored $BRIGHT_YELLOW "${BOLD}Please run this script from the project root directory${NC}"
    exit 1
fi

# Parse command line arguments
parse_arguments "$@"

# Run main function
main