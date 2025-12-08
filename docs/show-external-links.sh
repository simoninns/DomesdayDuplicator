#!/bin/bash

# Script to find and list all external links in markdown files
# Searches for both markdown-style links [text](http://...) and HTML <a href="http://..."> tags

echo "Searching for external links in .md files..."
echo ""

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
        echo "File: $file"
        echo "----------------------------------------"
        
        if [ -n "$markdown_links" ]; then
            echo "Markdown links:"
            echo "$markdown_links" | while read -r link; do
                # Extract URL from markdown link
                url=$(echo "$link" | grep -oP '\((https?://[^\)]+)\)' | tr -d '()')
                echo "  - $url"
            done
        fi
        
        if [ -n "$html_links" ]; then
            echo "HTML anchor links:"
            echo "$html_links" | while read -r link; do
                # Extract URL from HTML href
                url=$(echo "$link" | grep -oP 'href=["'"'"']\K(https?://[^"'"'"']+)')
                echo "  - $url"
            done
        fi
        
        if [ -n "$img_links" ]; then
            echo "HTML image links:"
            echo "$img_links" | while read -r link; do
                # Extract URL from HTML src
                url=$(echo "$link" | grep -oP 'src=["'"'"']\K(https?://[^"'"'"']+)')
                echo "  - $url"
            done
        fi
        
        echo ""
    fi
done

echo "Search complete."
