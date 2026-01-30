#ifndef PDF_FILLER_H
#define PDF_FILLER_H

#include <string>
#include <vector>
#include <memory>
#include <cstdint>

namespace pdffiller {

// Field types matching PDF spec
enum class FieldType {
    Unknown = 0,
    Text,
    Button,      // Push button
    Checkbox,
    Radio,
    Choice,      // Dropdown/Listbox
    Signature
};

// Form field information (named PdfFormField to avoid collision with Poppler's FormField)
struct PdfFormField {
    std::string name;
    std::string fullName;      // Fully qualified name (parent.child)
    std::string value;
    std::string defaultValue;
    FieldType type = FieldType::Unknown;
    bool readOnly = false;
    bool required = false;
    int pageIndex = 0;

    // Geometry (in PDF points, origin at bottom-left)
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    // For choice fields
    std::vector<std::string> options;

    // For checkboxes/radios
    std::string exportValue;   // Value when checked
    bool isChecked = false;
};

// Document handle
class PdfDocument {
public:
    PdfDocument();
    ~PdfDocument();

    // Load PDF from memory buffer
    bool loadFromMemory(const uint8_t* data, size_t length, const std::string& password = "");

    // Load PDF from virtual filesystem path (Emscripten FS)
    bool loadFromFile(const std::string& path, const std::string& password = "");

    // Get document info
    int getPageCount() const;
    std::string getTitle() const;
    std::string getAuthor() const;
    bool hasAcroForm() const;

    // Get all form fields
    std::vector<PdfFormField> getFormFields() const;

    // Get field by name
    PdfFormField* getFieldByName(const std::string& name);

    // Set field values
    bool setFieldValue(const std::string& name, const std::string& value);
    bool setCheckboxValue(const std::string& name, bool checked);

    // Bulk set fields from JSON-like structure
    bool setFieldValues(const std::vector<std::pair<std::string, std::string>>& values);

    // Flatten form (make fields non-editable, embed into page content)
    bool flattenForm();

    // Save to memory buffer
    std::vector<uint8_t> saveToMemory() const;

    // Save to virtual filesystem
    bool saveToFile(const std::string& path) const;

    // Render a page to PNG (for preview)
    std::vector<uint8_t> renderPageToPng(int pageIndex, double dpi = 150.0) const;

    // Get last error message
    std::string getLastError() const;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

// Utility functions
std::string fieldTypeToString(FieldType type);
FieldType stringToFieldType(const std::string& str);

} // namespace pdffiller

#endif // PDF_FILLER_H
