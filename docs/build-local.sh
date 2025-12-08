#!/bin/bash

# Local build script for testing documentation
# Builds the site to mockup/ folder for local browsing

set -e

echo "Building documentation to mockup/ folder..."

# Check if Jekyll is installed
if ! command -v jekyll &> /dev/null; then
    echo "❌ Jekyll not found."
    echo "Please see README.md for installation instructions for your OS."
    echo "Make sure to install: jekyll, bundler, and jekyll-theme-cayman"
    exit 1
fi

# Prepare temporary copy so we never mutate source files
TMP_SRC="$(mktemp -d)"
cleanup() {
    rm -rf "$TMP_SRC"
}
trap cleanup EXIT

# Copy source into temp build context
rsync -a wiki-default/ "$TMP_SRC/"

# Ensure every markdown file is processed by Jekyll without changing the originals
find "$TMP_SRC" -name "*.md" -type f ! -path "*/_*" | while read -r f; do
    if ! head -1 "$f" | grep -q '^---'; then
        # Inject minimal front matter so Jekyll will render to HTML
        sed -i '1i ---\nlayout: default\n---\n' "$f"
    fi
done

# Remove old mockup
rm -rf mockup/

# Build with Jekyll from the temp copy, overriding baseurl to empty for local testing
jekyll build -d mockup -s "$TMP_SRC" --config "$TMP_SRC/_config.yml" --baseurl ""

# Get absolute path to mockup folder
MOCKUP_PATH="$(cd mockup && pwd)/index.html"

echo ""
echo "✓ Build complete!"
echo ""
echo "To view the site locally, start a web server:"
echo "  cd mockup && python3 -m http.server 8000"
echo "Then open: http://localhost:8000"
echo ""
