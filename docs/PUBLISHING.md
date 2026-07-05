# Publishing OTAA Arduino Library

This guide explains how to publish the OTAA library to Arduino Library Manager and PlatformIO Registry.

## Prerequisites

1. GitHub account
2. Arduino account (for Library Manager)
3. PlatformIO account (for PlatformIO Registry)

## Step 1: Create GitHub Repository

```bash
# Navigate to library directory
cd /Users/ray/workspace/otaa/OTAA-Arduino

# Initialize git
git init
git add .
git commit -m "Initial commit v1.0.0"

# Create GitHub repository
gh repo create OTAA-Arduino --public --source=. --push

# Create release
gh release create v1.0.0 --title "v1.0.0" --notes "Initial release"
```

## Step 2: Submit to Arduino Library Manager

### Option A: Via GitHub Issue

1. Go to https://github.com/arduino/library-registry
2. Click "New Issue"
3. Use this template:

```
**Library Name:** OTAA
**Library URL:** https://github.com/otaa-platform/OTAA-Arduino
**Description:** OTA update library for ESP32/ESP8266 devices
**Category:** Communication
**Architecture:** esp32, esp8266
```

### Option B: Via Pull Request

1. Fork https://github.com/arduino/library-registry
2. Clone your fork
3. Add your library to `registry.txt`:
   ```
   https://github.com/otaa-platform/OTAA-Arduino
   ```
4. Create Pull Request

### Review Process

- Arduino team will review your library
- They check:
  - library.properties is valid
  - Code compiles
  - Examples work
  - Documentation is complete
- Usually takes 1-2 weeks

## Step 3: Submit to PlatformIO Registry

### Option A: Via PlatformIO CLI

```bash
# Install PlatformIO CLI
pip install platformio

# Login
pio account login

# Publish
pio package publish
```

### Option B: Via Website

1. Go to https://platformio.org/lib
2. Click "Add Library"
3. Enter GitHub URL: https://github.com/otaa-platform/OTAA-Arduino
4. Fill in details:
   - Name: OTAA
   - Description: OTA update library for IoT devices
   - Keywords: ota, update, esp32, esp8266, iot
   - Repository: https://github.com/otaa-platform/OTAA-Arduino
   - License: MIT
   - Frameworks: Arduino
   - Platforms: Espressif32, Espressif8266

### Automatic Publishing

GitHub Actions will automatically publish when you create a release:

```bash
# Update version
./scripts/release.sh 1.1.0

# Push
git push origin main --tags
```

## Step 4: Update Documentation

### Update README.md

1. Add badges:
   ```markdown
   [![Arduino Library](https://www.ardu-badge.com/badge/OTAA.svg)](https://www.ardu-badge.com/OTAA)
   [![PlatformIO](https://img.shields.io/badge/PlatformIO-Library-orange.svg)](https://platformio.org/lib/show/OTAA)
   ```

2. Update installation instructions

3. Add usage examples

### Create Documentation Site

Optionally, create a documentation site using GitHub Pages:

1. Create `docs/` folder
2. Add `index.html` with documentation
3. Enable GitHub Pages in repository settings

## Step 5: Promote Your Library

### Share on Social Media

- Twitter/X
- Reddit (r/arduino, r/esp32)
- Arduino Forum
- PlatformIO Community

### Write Blog Post

- Explain what the library does
- Show examples
- Link to documentation

### Create Video Tutorial

- How to install
- Basic usage
- Advanced features

## Versioning

Follow Semantic Versioning:

- **MAJOR**: Breaking changes
- **MINOR**: New features (backward compatible)
- **PATCH**: Bug fixes

Example:
```bash
# Patch release (bug fix)
./scripts/release.sh 1.0.1

# Minor release (new feature)
./scripts/release.sh 1.1.0

# Major release (breaking change)
./scripts/release.sh 2.0.0
```

## Checklist Before Publishing

- [ ] library.properties is complete and valid
- [ ] All examples compile
- [ ] README.md is complete
- [ ] LICENSE file exists
- [ ] keywords.txt is complete
- [ ] Version number is correct
- [ ] All tests pass
- [ ] Documentation is complete
- [ ] GitHub repository is public
- [ ] Release is created

## Common Issues

### Library Not Appearing in Manager

- Wait 24-48 hours after submission
- Check library.properties is valid
- Verify GitHub repository is public
- Check for spelling errors

### PlatformIO Build Fails

- Check platformio.ini is correct
- Verify dependencies are listed
- Test locally first

### Version Conflicts

- Always update version before releasing
- Use semantic versioning
- Don't reuse version numbers

## Support

For issues with the library:
- GitHub Issues: https://github.com/otaa-platform/OTAA-Arduino/issues
- Email: support@otaa.dev

For Arduino Library Manager issues:
- Arduino Forum: https://forum.arduino.cc/
- GitHub: https://github.com/arduino/library-registry/issues

For PlatformIO issues:
- PlatformIO Community: https://community.platformio.org/
- GitHub: https://github.com/platformio/platformio-core/issues
