#!/bin/bash

# Script to find orphaned pages - .md files not linked in Sidebar.md
# These pages exist but are not accessible through the main navigation

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

SIDEBAR_FILE="wiki-default/Sidebar.md"

# Check if Sidebar.md exists
if [[ ! -f "$SIDEBAR_FILE" ]]; then
    echo -e "${RED}Error: $SIDEBAR_FILE not found${NC}"
    exit 1
fi

echo "Checking for orphaned pages in wiki-default/..."
echo ""

# Extract all .md file links from Sidebar.md (normalize paths)
linked_pages=()
while IFS= read -r link; do
    # Skip empty lines
    [[ -z "$link" ]] && continue
    
    # Remove leading/trailing whitespace
    link=$(echo "$link" | xargs)
    
    # Normalize path - remove leading ./ and wiki-default/
    link="${link#./}"
    link="${link#wiki-default/}"
    
    linked_pages+=("$link")
done < <(perl -nle 'while (/\]\(([^)]+(?:\([^)]*\)[^)]*)*)\)/g) { print $1 }' "$SIDEBAR_FILE" 2>/dev/null | grep '\.md$')

# Find all .md files in wiki-default (excluding Sidebar.md, Footer.md, _* files)
orphaned_pages=()
total_pages=0

while IFS= read -r -d '' md_file; do
    # Get relative path from wiki-default/
    rel_path="${md_file#wiki-default/}"
    
    # Skip special files
    if [[ "$rel_path" == "Sidebar.md" ]] || \
       [[ "$rel_path" == "Footer.md" ]] || \
       [[ "$rel_path" == _* ]] || \
       [[ "$rel_path" == */_* ]]; then
        continue
    fi
    
    total_pages=$((total_pages + 1))
    
    # Check if this page is linked in Sidebar
    found=false
    for linked in "${linked_pages[@]}"; do
        if [[ "$linked" == "$rel_path" ]]; then
            found=true
            break
        fi
    done
    
    if [[ "$found" == false ]]; then
        orphaned_pages+=("$rel_path")
    fi
done < <(find wiki-default -name "*.md" -type f -print0)

echo "===================="
echo "Summary:"
echo "===================="
echo -e "Total pages (excluding special files): ${total_pages}"
echo -e "Pages linked in Sidebar: ${#linked_pages[@]}"
echo -e "Orphaned pages: ${#orphaned_pages[@]}"
echo ""

if [[ ${#orphaned_pages[@]} -eq 0 ]]; then
    echo -e "${GREEN}✓ No orphaned pages found - all pages are linked in Sidebar.md${NC}"
    exit 0
else
    echo -e "${YELLOW}⚠ Found ${#orphaned_pages[@]} orphaned page(s):${NC}"
    echo ""
    for page in "${orphaned_pages[@]}"; do
        echo -e "  ${BLUE}•${NC} $page"
    done
    echo ""
    echo -e "${YELLOW}Note: These pages exist but are not accessible through the sidebar navigation.${NC}"
    echo -e "${YELLOW}Consider adding them to Sidebar.md or moving them to the Orphans/ directory.${NC}"
    exit 0
fi
