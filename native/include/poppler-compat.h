#ifndef POPPLER_COMPAT_H
#define POPPLER_COMPAT_H

// Compatibility layer for different Poppler versions
// Poppler's API changes frequently, so we need to handle version differences

#include <poppler/poppler-config.h>

// Check Poppler version
#ifndef POPPLER_VERSION
#define POPPLER_VERSION "0.0.0"
#endif

// Version comparison macros
#define POPPLER_VERSION_ENCODE(major, minor, micro) \
    (((major) * 10000) + ((minor) * 100) + (micro))

// Parse version string at compile time is tricky, so we use runtime checks
// or rely on API presence

// In newer Poppler (22.x+), some APIs changed:
// - GooString is used differently
// - Object API changed to use constructors instead of init* methods
// - Password handling uses std::optional<GooString>

// For PDFDoc constructor compatibility:
// Old: PDFDoc(GooString*, GooString*, GooString*)
// New: PDFDoc(Stream*, std::optional<GooString>, std::optional<GooString>)

// For Form field content:
// - getContent() returns const GooString* in older versions
// - In newer versions, it may return differently

// For saving:
// - saveAs(GooString*, PDFWriteMode) is consistent
// - But writeMode enum values may differ

// Splash rendering is fairly stable

#endif // POPPLER_COMPAT_H
