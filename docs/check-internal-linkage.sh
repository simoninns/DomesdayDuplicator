#!/bin/bash

# Script to check internal markdown links in the wiki-default directory
# Verifies that all internal .md links point to existing files

set -e

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Counters
total_links=0
broken_links=0
missing_files=()

echo "Checking internal markdown links in wiki-default/..."
echo ""

# Find all .md files in wiki-default directory
while IFS= read -r -d '' md_file; do
    # Get the directory of the current file for resolving relative paths
    file_dir=$(dirname "$md_file")
    
    # Extract markdown links: [text](path)
    # This regex captures internal links (not starting with http:// or https://)
    # Using a more careful regex to handle parentheses in filenames
    while IFS= read -r link; do
        # Skip empty lines
        [[ -z "$link" ]] && continue
        
        # Skip external links (http/https)
        if [[ "$link" =~ ^https?:// ]]; then
            continue
        fi
        
        # Skip anchor-only links (#section)
        if [[ "$link" =~ ^# ]]; then
            continue
        fi
        
        # Remove anchor fragments from the link (e.g., file.md#section -> file.md)
        link_without_anchor="${link%%#*}"
        
        # Remove title/alt text (e.g., "image.png "title"" -> "image.png")
        link_without_anchor=$(echo "$link_without_anchor" | sed 's/[[:space:]]*"[^"]*"[[:space:]]*$//')
        
        # Skip image extensions (png, jpg, jpeg, gif, svg, pdf)
        if [[ "$link_without_anchor" =~ \.(png|jpg|jpeg|gif|svg|pdf)$ ]]; then
            continue
        fi
        
        # Skip if link is empty after removing anchor
        [[ -z "$link_without_anchor" ]] && continue
        
        total_links=$((total_links + 1))
        
        # Check if link is missing .md extension (likely an internal page link)
        # but not an image or external resource
        needs_md_ext=false
        if [[ ! "$link_without_anchor" =~ \.md$ ]] && [[ ! "$link_without_anchor" =~ ^mailto: ]]; then
            needs_md_ext=true
        fi
        
        # Try to resolve the file path
        target_file=""
        
        # Resolve the path relative to the current file's directory
        if [[ "$link_without_anchor" =~ ^/ ]]; then
            # Absolute path from wiki-default root
            target_file="wiki-default${link_without_anchor}"
        else
            # Relative path
            target_file="${file_dir}/${link_without_anchor}"
        fi
        
        # If link is missing .md extension, try adding it
        if [[ "$needs_md_ext" == true ]]; then
            target_file="${target_file}.md"
        fi
        
        # Normalize the path (resolve .. and .)
        target_file=$(realpath -m "$target_file")
        
        # Check if the target file exists
        if [[ ! -f "$target_file" ]]; then
            broken_links=$((broken_links + 1))
            echo -e "${RED}✗${NC} Broken link in ${YELLOW}${md_file}${NC}"
            if [[ "$needs_md_ext" == true ]]; then
                echo -e "  Link: ${link} ${YELLOW}(missing .md extension?)${NC}"
            else
                echo -e "  Link: ${link}"
            fi
            echo -e "  Expected file: ${target_file}"
            echo ""
            missing_files+=("$md_file -> $link")
        fi
    done < <(
        # Extract markdown links more carefully to handle parentheses in filenames
        # Match ](url) but capture everything between ]( and the last )
        perl -nle 'while (/\]\(([^)]+(?:\([^)]*\)[^)]*)*)\)/g) { print $1 }' "$md_file" 2>/dev/null || true
    )
    
done < <(find wiki-default -name "*.md" -type f -print0)

echo "===================="
echo "Summary:"
echo "===================="
echo -e "Total internal .md links checked: ${total_links}"

if [[ $broken_links -eq 0 ]]; then
    echo -e "${GREEN}✓ All internal markdown links are valid!${NC}"
    exit 0
else
    echo -e "${RED}✗ Found ${broken_links} broken link(s)${NC}"
    echo ""
    echo "Broken links:"
    for item in "${missing_files[@]}"; do
        echo -e "  ${RED}•${NC} $item"
    done
    exit 1
fi
