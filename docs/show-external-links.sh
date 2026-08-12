#!/bin/bash

# Script to find and list all external links in markdown files
# Also validates HTTP status codes for external links
# Searches for both markdown-style links [text](http://...) and HTML <a href="http://..."> tags

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Options
CHECK_LINKS=false
TIMEOUT=5

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --check)
            CHECK_LINKS=true
            shift
            ;;
        --timeout)
            TIMEOUT="$2"
            shift 2
            ;;
        *)
            shift
            ;;
    esac
done

echo "Searching for external links in .md files..."
if [[ "$CHECK_LINKS" == true ]]; then
    echo "HTTP status checking enabled (timeout: ${TIMEOUT}s)"
fi
echo ""

# Track results
total_links=0
checked_links=0
broken_links=0
error_links=()

# Find all .md files and search for external links
find wiki-default -name "*.md" -type f | while read -r file; do
    # Search for markdown-style links with http:// or https://
    markdown_links=$(grep -oP '\[([^\]]+)\]\((https?://[^\)]+)\)' "$file" 2>/dev/null || true)
    
    # Search for HTML anchor tags with http:// or https://
    html_links=$(grep -oP '<a\s+[^>]*href=["'"'"'](https?://[^"'"'"']+)["'"'"'][^>]*>' "$file" 2>/dev/null || true)
    
    # Search for HTML img tags with http:// or https://
    img_links=$(grep -oP '<img\s+[^>]*src=["'"'"'](https?://[^"'"'"']+)["'"'"'][^>]*>' "$file" 2>/dev/null || true)
    
    # If any external links found, display them
    if [ -n "$markdown_links" ] || [ -n "$html_links" ] || [ -n "$img_links" ]; then
        echo -e "File: ${BLUE}$file${NC}"
        echo "----------------------------------------"
        
        if [ -n "$markdown_links" ]; then
            echo "Markdown links:"
            echo "$markdown_links" | while read -r link; do
                # Extract URL from markdown link
                url=$(echo "$link" | grep -oP '\((https?://[^\)]+)\)' | tr -d '()')
                
                if [[ "$CHECK_LINKS" == true ]] && [[ ! -z "$url" ]]; then
                    # Check HTTP status
                    status=$(curl -s -o /dev/null -w "%{http_code}" --max-time "$TIMEOUT" "$url" 2>/dev/null || echo "000")
                    
                    if [[ "$status" == "200" ]]; then
                        echo -e "  - ${GREEN}✓${NC} $url (HTTP $status)"
                    elif [[ "$status" == "000" ]]; then
                        echo -e "  - ${RED}✗${NC} $url (${RED}Connection error${NC})"
                    elif [[ "$status" =~ ^[45] ]]; then
                        echo -e "  - ${RED}✗${NC} $url (HTTP ${RED}$status${NC})"
                    else
                        echo -e "  - ${YELLOW}⚠${NC} $url (HTTP $status)"
                    fi
                else
                    echo "  - $url"
                fi
            done
        fi
        
        if [ -n "$html_links" ]; then
            echo "HTML anchor links:"
            echo "$html_links" | while read -r link; do
                # Extract URL from HTML href
                url=$(echo "$link" | grep -oP 'href=["'"'"']\K(https?://[^"'"'"']+)')
                
                if [[ "$CHECK_LINKS" == true ]] && [[ ! -z "$url" ]]; then
                    # Check HTTP status
                    status=$(curl -s -o /dev/null -w "%{http_code}" --max-time "$TIMEOUT" "$url" 2>/dev/null || echo "000")
                    
                    if [[ "$status" == "200" ]]; then
                        echo -e "  - ${GREEN}✓${NC} $url (HTTP $status)"
                    elif [[ "$status" == "000" ]]; then
                        echo -e "  - ${RED}✗${NC} $url (${RED}Connection error${NC})"
                    elif [[ "$status" =~ ^[45] ]]; then
                        echo -e "  - ${RED}✗${NC} $url (HTTP ${RED}$status${NC})"
                    else
                        echo -e "  - ${YELLOW}⚠${NC} $url (HTTP $status)"
                    fi
                else
                    echo "  - $url"
                fi
            done
        fi
        
        if [ -n "$img_links" ]; then
            echo "HTML image links:"
            echo "$img_links" | while read -r link; do
                # Extract URL from HTML src
                url=$(echo "$link" | grep -oP 'src=["'"'"']\K(https?://[^"'"'"']+)')
                
                if [[ "$CHECK_LINKS" == true ]] && [[ ! -z "$url" ]]; then
                    # Check HTTP status
                    status=$(curl -s -o /dev/null -w "%{http_code}" --max-time "$TIMEOUT" "$url" 2>/dev/null || echo "000")
                    
                    if [[ "$status" == "200" ]]; then
                        echo -e "  - ${GREEN}✓${NC} $url (HTTP $status)"
                    elif [[ "$status" == "000" ]]; then
                        echo -e "  - ${RED}✗${NC} $url (${RED}Connection error${NC})"
                    elif [[ "$status" =~ ^[45] ]]; then
                        echo -e "  - ${RED}✗${NC} $url (HTTP ${RED}$status${NC})"
                    else
                        echo -e "  - ${YELLOW}⚠${NC} $url (HTTP $status)"
                    fi
                else
                    echo "  - $url"
                fi
            done
        fi
        
        echo ""
    fi
done

echo "Search complete."
if [[ "$CHECK_LINKS" == true ]]; then
    echo ""
    echo -e "${GREEN}✓${NC} = Valid (HTTP 200)"
    echo -e "${YELLOW}⚠${NC} = Warning (HTTP 3xx redirect)"
    echo -e "${RED}✗${NC} = Error (HTTP 4xx/5xx or connection failed)"
fi

