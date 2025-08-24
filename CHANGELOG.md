# Changelog

This project follows [Semantic Versioning](https://semver.org/). Each entry lists a version tag, release date, and grouped changes. Document every update and include notes on common errors and how they were fixed to preserve the project's evolution.

## [Unreleased]

### Added
- Initial changelog to guide future entries.

### Changed
- Switched to the official `webview` port and removed the custom overlay.

### Errors and Fixes
- ImGui optional docking/viewport flags (`ImGuiConfigFlags_DockingEnable`, `ImGuiConfigFlags_ViewportsEnable`) caused build errors with the vcpkg package. The flags and related calls were removed to restore successful compilation.
- Overlay port `webview` failed to build due to deprecated `vcpkg_copy_sources`; replaced with direct `file(INSTALL ...)` commands.

For every future change:
1. Add a new version section with the format `## [x.y.z] - YYYY-MM-DD`.
2. Document the change under categories such as **Added**, **Changed**, **Fixed**, or **Removed**.
3. Note any encountered errors and the steps taken to resolve them under **Errors and Fixes**.
