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

# Extended colors for more vibrant output
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

# Background colors for emphasis
BG_RED='\033[41m'
BG_GREEN='\033[42m'
BG_YELLOW='\033[43m'
BG_BLUE='\033[44m'
BG_MAGENTA='\033[45m'
BG_CYAN='\033[46m'

# Special effects
BOLD='\033[1m'
DIM='\033[2m'
UNDERLINE='\033[4m'
BLINK='\033[5m'
REVERSE='\033[7m'

# Function to print colored output
print_colored() {
    echo -e "${1}${2}${NC}"
}

# Function to print section headers
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
    # 1) Capturamos la línea de código: "  191 |    struct…"
    if [[ $line =~ ^([[:space:]]*[0-9]+[[:space:]]*\|)(.*)$ ]]; then
      prefix="${BASH_REMATCH[1]}"
      code_line="${BASH_REMATCH[2]}"
      # Colorear el número de línea
      if [[ $prefix =~ ^([[:space:]]*)([0-9]+)([[:space:]]*\|)$ ]]; then
        local spaces="${BASH_REMATCH[1]}"
        local line_num="${BASH_REMATCH[2]}"
        local pipe="${BASH_REMATCH[3]}"
        prefix="${spaces}${BRIGHT_BLUE}${line_num}${TEAL}${pipe}${NC}"
      fi
      continue
    fi

    # 2) Al encontrar los carets, coloreamos sólo el fragmento
    if [[ $line =~ ^[[:space:]]*\|([[:space:]]*)(\^[~^]*|~+)(.*)$ ]]; then
      local indent=${#BASH_REMATCH[1]}    # cuántos espacios tras la barra
      local caret_part="${BASH_REMATCH[2]}"  # los carets y tildes
      local clen=${#caret_part}           # longitud total de carets+tildes
      
      # trozos: antes | fragmento | después
      local before=${code_line:0:indent}
      local frag=${code_line:indent:clen}
      local after=${code_line:indent+clen}

      # imprimimos la línea de código con sólo frag en rojo brillante con fondo
      echo -e "${prefix}${before}${BG_RED}${BRIGHT_YELLOW}${frag}${NC}${after}"
      # y volvemos a pintar la línea de carets también en rojo brillante
      echo -e "${BRIGHT_RED}${BLINK}${line}${NC}"

      # limpiamos para el siguiente bloque
      prefix=""; code_line=""
      continue
    fi

    # 3) Colorear líneas de warning/error/note con más colores
    if [[ $line =~ ^(src/[^:]+):([0-9]+):([0-9]+):[[:space:]]*(warning|error|note|fatal error):[[:space:]]*(.*)$ ]]; then
      local file="${BASH_REMATCH[1]}"
      local line_no="${BASH_REMATCH[2]}"
      local col_no="${BASH_REMATCH[3]}"
      local type="${BASH_REMATCH[4]}"
      local message="${BASH_REMATCH[5]}"
      
      # Colorear el archivo
      echo -ne "${BRIGHT_CYAN}${file}${NC}:${BRIGHT_MAGENTA}${line_no}${NC}:${BRIGHT_MAGENTA}${col_no}${NC}: "
      
      case "$type" in
        "warning") echo -e "${BG_YELLOW}${BOLD} WARNING ${NC} ${ORANGE}${message}${NC}" ;;
        "error") echo -e "${BG_RED}${BRIGHT_YELLOW}${BOLD} ERROR ${NC} ${BRIGHT_RED}${message}${NC}" ;;
        "fatal error") echo -e "${BG_RED}${BRIGHT_WHITE}${BOLD} FATAL ERROR ${NC} ${BRIGHT_RED}${UNDERLINE}${message}${NC}" ;;
        "note") echo -e "${BG_BLUE}${BRIGHT_WHITE}${BOLD} NOTE ${NC} ${BRIGHT_BLUE}${message}${NC}" ;;
      esac
      continue
    fi

    # 4) Colorear otras líneas importantes
    if [[ $line =~ ^(In function|At top level|In file included from) ]]; then
      echo -e "${PURPLE}${BOLD}${line}${NC}"
      continue
    fi

    # 5) Cualquier otra línea
    echo "$line"
  done
}

# Function to print ASCII art
print_ascii_art() {
    print_colored $BRIGHT_MAGENTA "
${BRIGHT_CYAN}███████╗${BRIGHT_MAGENTA} ██████╗ ${BRIGHT_YELLOW} ██████╗${BRIGHT_GREEN}██╗  ██╗${BRIGHT_BLUE}███████╗
${BRIGHT_CYAN}██╔════╝${BRIGHT_MAGENTA}██╔═══██╗${BRIGHT_YELLOW}██╔════╝${BRIGHT_GREEN}██║ ██╔╝${BRIGHT_BLUE}██╔════╝
${BRIGHT_CYAN}███████╗${BRIGHT_MAGENTA}██║   ██║${BRIGHT_YELLOW}██║     ${BRIGHT_GREEN}█████╔╝ ${BRIGHT_BLUE}███████╗
${BRIGHT_CYAN}╚════██║${BRIGHT_MAGENTA}██║   ██║${BRIGHT_YELLOW}██║     ${BRIGHT_GREEN}██╔═██╗ ${BRIGHT_BLUE}╚════██║
${BRIGHT_CYAN}███████║${BRIGHT_MAGENTA}╚██████╔╝${BRIGHT_YELLOW}╚██████╗${BRIGHT_GREEN}██║  ██╗${BRIGHT_BLUE}███████║
${BRIGHT_CYAN}╚══════╝${BRIGHT_MAGENTA} ╚═════╝ ${BRIGHT_YELLOW} ╚═════╝${BRIGHT_GREEN}╚═╝  ╚═╝${BRIGHT_BLUE}╚══════╝
"
    print_colored $LIME "            Ready to Rock and Roll!"
    echo
}

# Function to show build statistics
show_build_stats() {
    local start_time=$1
    local end_time=$2
    local duration=$((end_time - start_time))
    
    echo
    print_colored $BRIGHT_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    print_colored $BG_BLUE$BRIGHT_WHITE$BOLD "                     BUILD STATISTICS                     "
    print_colored $BRIGHT_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
    printf "${BRIGHT_YELLOW}Build time: ${BRIGHT_GREEN}%02d:%02d${NC}\n" $((duration / 60)) $((duration % 60))
    printf "${BRIGHT_YELLOW}Object files: ${BRIGHT_GREEN}%d${NC}\n" $(find src -name '*.o' | wc -l)
    printf "${BRIGHT_YELLOW}Executable: ${BRIGHT_GREEN}%s${NC}\n" "$(ls -lh main 2>/dev/null | awk '{print $5}' || echo 'N/A')"
    print_colored $BRIGHT_CYAN "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
}

# Main build process
main() {
    local start_time=$(date +%s)
    
    print_colored $BRIGHT_MAGENTA "${BOLD}Starting SOCKS5 Build Process...${NC}"
    
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
    
    # Build phase
    print_header "BUILDING PROJECT"
    print_colored $BRIGHT_YELLOW "${BOLD}Compiling source files...${NC}"
    echo
    
    # Capture build output and format it
    if make all 2>&1 | format_warnings; then
        local end_time=$(date +%s)
        echo
        print_colored $BG_GREEN$BRIGHT_WHITE$BOLD " Build completed successfully! "
        
        # Show build statistics
        show_build_stats $start_time $end_time
        
        # Show ASCII art
        print_ascii_art
        
        print_colored $BRIGHT_GREEN "${BOLD}Ready to execute: ${BRIGHT_CYAN}./socks5d${NC}"
        print_colored $BRIGHT_BLUE "${BOLD}Run tests with: ${BRIGHT_CYAN}./test.sh${NC}"
        
    else
        print_colored $BG_RED$BRIGHT_WHITE$BOLD " Build failed! "
        print_colored $BRIGHT_YELLOW "${BOLD}Check the errors above and fix them before proceeding.${NC}"
        exit 1
    fi
}

# Check if we're in the right directory
if [[ ! -f "Makefile" ]]; then
    print_colored $BG_RED$BRIGHT_WHITE$BOLD " Error: Makefile not found in current directory "
    print_colored $BRIGHT_YELLOW "${BOLD}Please run this script from the project root directory${NC}"
    exit 1
fi