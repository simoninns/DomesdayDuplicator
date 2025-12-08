# ld-decode documentation contributing

## Contributing

PRs to this repository are automatically deployed to the GitHub Pages documentation site, providing a mechanism for contributors to add new documentation and make corrections to existing content.

> [!Important]
> If possible test your changes locally before submitting a PR (see [Testing](TESTING.md))

## How to contribute

To contribute to the documentation:

1. **Fork the repository** - Click the "Fork" button at the top right of this repository to create your own copy
2. **Clone your fork** - Clone your forked repository to your local machine:
   ```bash
   git clone https://github.com/YOUR-USERNAME/ld-decode-docs.git
   cd ld-decode-docs
   ```
3. **Create a branch** - Create a new branch for your changes:
   ```bash
   git checkout -b improve-documentation
   ```
4. **Make your changes** - Edit the markdown files in the `wiki-default/` directory
   - All documentation is written in Markdown format
   - Keep `.md` extensions in links - they are automatically converted to `.html` during build
5. **Test locally** (optional but recommended) - See [Testing](TESTING.md) for instructions
6. **Commit your changes** - Commit with a clear, descriptive message:
   ```bash
   git add .
   git commit -m "Add documentation for feature X"
   ```
7. **Push to your fork** - Push your branch to your GitHub fork:
   ```bash
   git push origin improve-documentation
   ```
8. **Create a Pull Request** - Go to the original repository and click "New Pull Request"
   - Select your fork and branch
   - Provide a clear description of your changes
   - Submit the PR for review

Once your PR is merged, your changes will automatically be deployed to the GitHub Pages site.

