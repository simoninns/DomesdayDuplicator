# ld-decode documentation testing

## Testing Changes Locally

### Testing Prerequisites - Ubuntu (24.04 LTS and later)

```bash
# Update package manager
sudo apt update

# Install Ruby and dependencies
sudo apt install -y ruby-full build-essential zlib1g-dev

# Install Jekyll, bundler, and required gems
sudo gem install jekyll bundler jekyll-theme-cayman jekyll-relative-links
```

#### Testing Prerequisites - Fedora (43 and later)

```bash
# Install Ruby and dependencies
sudo dnf install -y ruby ruby-devel gcc gcc-c++ make redhat-rpm-config zlib-devel

# Install Jekyll, bundler, and required gems
sudo gem install jekyll bundler jekyll-theme-cayman jekyll-relative-links
```

### Building the Site Locally

```bash
# Build documentation to mockup/ folder
./build-local.sh
```

This creates a `mockup/` folder with the built site ready for local testing.

### Adding or Editing Documentation

You can add plain Markdown files under `wiki-default/` without any front matter. The build pipeline automatically copies the files into a temporary workspace and injects minimal front matter so Jekyll will render them to HTML. Your source files stay untouched in the repository.

### Viewing the Site

To properly test the site with working links and search functionality, use a local web server:

```bash
# Navigate to the mockup folder
cd mockup

# Start a Python web server
python3 -m http.server 8000
```

Then open your browser and navigate to:
```
http://localhost:8000
```

The site will be served at `http://localhost:8000/index.html` with all links and features working correctly.

To stop the server, press `Ctrl+C` in the terminal.

> [!Note]
> Using a web server instead of the `file://` protocol ensures that absolute links, search functionality, and other features work correctly during local testing.

### Checking for External Links

You can verify that no external links have been accidentally added to the documentation:

```bash
# List all external links in markdown files
./show-external-links.sh
```

This script searches all `.md` files for external links (http:// or https://) in markdown-style links, HTML anchor tags, and image tags. The output will show which files contain external links and what they are. This is useful for quality assurance before committing changes.

> [!Important]
> Pay attention to your use of external links and consider any complexities around linking to forks of this documentation repository.  Wherever possible content should be local and forks can then modify content as required.

### Important Notes

- The `mockup/` folder is in `.gitignore` - it will never be committed
- Always build locally before pushing changes
- The navigation sidebar must not contain external links (validation enforced by CI)
- Use relative links for internal documentation pages