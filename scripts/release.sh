#!/bin/bash

# OTAA Arduino Library Release Script
# Usage: ./scripts/release.sh <version>

set -e

VERSION=$1

if [ -z "$VERSION" ]; then
    echo "Usage: ./scripts/release.sh <version>"
    echo "Example: ./scripts/release.sh 1.0.0"
    exit 1
fi

echo "=== Releasing OTAA Arduino Library v${VERSION} ==="

# Update version in library.properties
sed -i "s/version=.*/version=${VERSION}/" library.properties

# Update version in OTAA.h
sed -i "s/#define OTAA_VERSION \".*\"/#define OTAA_VERSION \"${VERSION}\"/" src/OTAA.h

# Git operations
git add .
git commit -m "Release v${VERSION}"
git tag -a "v${VERSION}" -m "Release v${VERSION}"

echo "=== Release v${VERSION} created ==="
echo ""
echo "Next steps:"
echo "1. Push to GitHub:"
echo "   git push origin main --tags"
echo ""
echo "2. Create GitHub Release:"
echo "   gh release create v${VERSION} --title \"v${VERSION}\" --notes \"Release v${VERSION}\""
echo ""
echo "3. Submit to Arduino Library Registry:"
echo "   - Fork https://github.com/arduino/library-registry"
echo "   - Add your library URL to the list"
echo "   - Create a Pull Request"
echo ""
echo "4. Submit to PlatformIO:"
echo "   - Go to https://platformio.org/lib"
echo "   - Register your library"
echo "   - Or use: pio package publish"
