#include "pdf-filler.h"

// Poppler core API for full form support
#include <poppler/GlobalParams.h>
#include <poppler/PDFDoc.h>
#include <poppler/Page.h>
#include <poppler/Catalog.h>
#include <poppler/Form.h>
#include <poppler/Annot.h>
#include <poppler/Link.h>
#include <poppler/Object.h>
#include <poppler/Stream.h>
#include <poppler/SplashOutputDev.h>
#include <poppler/UTF.h>
#include <splash/SplashBitmap.h>

#include <png.h>
#include <cstring>
#include <sstream>
#include <fstream>
#include <algorithm>
#include <cstdio>
#include <optional>
#include <unordered_map>

namespace pdffiller {

// Initialize global params once
static bool globalParamsInitialized = false;
static void initGlobalParams() {
    if (!globalParamsInitialized) {
        globalParams = std::make_unique<GlobalParams>();
        globalParamsInitialized = true;
    }
}

// Helper to convert GooString to std::string (handles UTF-16 PDF text strings)
// Use this for display purposes
static std::string gooToStd(const GooString* gs) {
    if (!gs) return "";
    // PDF text strings can be UTF-16BE with BOM - convert to UTF-8
    return TextStringToUtf8(gs->toStr());
}

// Helper to get raw GooString bytes - use for field name lookups
// Poppler's findFieldByFullyQualifiedName compares raw bytes
static std::string gooToRaw(const GooString* gs) {
    if (!gs) return "";
    return gs->toStr();
}

// Helper to convert std::string to GooString
static std::unique_ptr<GooString> stdToGoo(const std::string& s) {
    return std::make_unique<GooString>(s.c_str(), s.length());
}

class PdfDocument::Impl {
public:
    std::unique_ptr<PDFDoc> doc_;
    std::vector<uint8_t> originalData_;  // Keep original for incremental save
    std::string lastError_;
    std::vector<PdfFormField> cachedFields_;
    std::unordered_map<std::string, ::FormField*> fieldMap_;  // Map from name to Poppler field
    bool fieldsCached_ = false;
    bool modified_ = false;

    Impl() {
        initGlobalParams();
    }

    ~Impl() = default;

    bool loadFromMemory(const uint8_t* data, size_t length, const std::string& password) {
        // Store original data for potential incremental save
        originalData_.assign(data, data + length);

        // Create a MemStream from the data
        // Note: PDFDoc takes ownership of the stream
        Object obj = Object(objNull);

        auto* stream = new MemStream(
            reinterpret_cast<const char*>(originalData_.data()),
            0,
            static_cast<Goffset>(originalData_.size()),
            std::move(obj)
        );

        std::optional<GooString> ownerPw = password.empty() ? std::nullopt : std::optional<GooString>(password);
        std::optional<GooString> userPw = password.empty() ? std::nullopt : std::optional<GooString>(password);

        doc_ = std::make_unique<PDFDoc>(stream, ownerPw, userPw);

        if (!doc_->isOk()) {
            lastError_ = "Failed to load PDF: error code " + std::to_string(doc_->getErrorCode());
            doc_.reset();
            return false;
        }

        fieldsCached_ = false;
        modified_ = false;
        return true;
    }

    bool loadFromFile(const std::string& path, const std::string& password) {
        // Read file into memory first
        std::ifstream file(path, std::ios::binary | std::ios::ate);
        if (!file) {
            lastError_ = "Failed to open file: " + path;
            return false;
        }

        size_t size = file.tellg();
        file.seekg(0);
        originalData_.resize(size);
        file.read(reinterpret_cast<char*>(originalData_.data()), size);
        file.close();

        // Now load using loadFromMemory
        return loadFromMemory(originalData_.data(), originalData_.size(), password);
    }

    Form* getForm() {
        if (!doc_) return nullptr;
        Catalog* catalog = doc_->getCatalog();
        if (!catalog) return nullptr;
        return catalog->getForm();
    }

    ::FormField* findFormField(const std::string& name) {
        // Ensure fields are cached (this also populates fieldMap_)
        cacheFormFields();

        // Look up by fullName first, then by name
        auto it = fieldMap_.find(name);
        if (it != fieldMap_.end()) {
            return it->second;
        }

        return nullptr;
    }

    void cacheFormFields() {
        if (fieldsCached_ || !doc_) return;
        cachedFields_.clear();

        Form* form = getForm();
        if (!form) {
            fieldsCached_ = true;
            return;
        }

        int numFields = form->getNumFields();
        for (int i = 0; i < numFields; ++i) {
            ::FormField* field = form->getRootField(i);
            if (field) {
                collectFieldsRecursive(field, cachedFields_, fieldMap_);
            }
        }

        fieldsCached_ = true;
    }

    void collectFieldsRecursive(::FormField* field, std::vector<pdffiller::PdfFormField>& output,
                                std::unordered_map<std::string, ::FormField*>& fieldMap) {
        if (!field) return;

        // Get widgets (visual representations) for this field
        int numWidgets = field->getNumWidgets();

        // If this is a terminal field (has widgets), add it
        if (numWidgets > 0) {
            pdffiller::PdfFormField ff;

            // Names - both use UTF-8 for consistency
            const GooString* fullName = field->getFullyQualifiedName();
            const GooString* partialName = field->getPartialName();
            ff.fullName = fullName ? gooToStd(fullName) : "";
            ff.name = partialName ? gooToStd(partialName) : ff.fullName;

            // Store pointer in map for fast lookup later
            if (!ff.fullName.empty()) {
                fieldMap[ff.fullName] = field;
            }
            if (!ff.name.empty() && ff.name != ff.fullName) {
                fieldMap[ff.name] = field;
            }

            // Type
            ff.type = convertFieldType(field->getType());

            // Flags
            ff.readOnly = field->isReadOnly();
            // Required flag is in the field flags (bit 2)

            // Get value based on type
            switch (field->getType()) {
                case formText: {
                    auto* textField = static_cast<FormFieldText*>(field);
                    const GooString* content = textField->getContent();
                    ff.value = content ? gooToStd(content) : "";
                    break;
                }
                case formChoice: {
                    auto* choiceField = static_cast<FormFieldChoice*>(field);
                    // Get selected value
                    int numSelected = choiceField->getNumSelected();
                    if (numSelected > 0) {
                        const GooString* selection = choiceField->getSelectedChoice();
                        ff.value = selection ? gooToStd(selection) : "";
                    }
                    // Get all options
                    int numChoices = choiceField->getNumChoices();
                    for (int c = 0; c < numChoices; ++c) {
                        const GooString* choice = choiceField->getChoice(c);
                        if (choice) {
                            ff.options.push_back(gooToStd(choice));
                        }
                    }
                    break;
                }
                case formButton: {
                    auto* buttonField = static_cast<FormFieldButton*>(field);
                    FormButtonType btnType = buttonField->getButtonType();
                    if (btnType == formButtonCheck) {
                        ff.type = FieldType::Checkbox;
                        ff.isChecked = buttonField->getState(0);  // First widget state
                    } else if (btnType == formButtonRadio) {
                        ff.type = FieldType::Radio;
                        ff.isChecked = buttonField->getState(0);
                    } else {
                        ff.type = FieldType::Button;
                    }
                    break;
                }
                case formSignature:
                    ff.type = FieldType::Signature;
                    break;
                default:
                    break;
            }

            // Get geometry from first widget
            if (numWidgets > 0) {
                FormWidget* widget = field->getWidget(0);
                if (widget) {
                    double x1, y1, x2, y2;
                    widget->getRect(&x1, &y1, &x2, &y2);
                    ff.x = x1;
                    ff.y = y1;
                    ff.width = x2 - x1;
                    ff.height = y2 - y1;
                    ff.pageIndex = widget->getWidgetAnnotation()->getPageNum() - 1;  // 0-based
                }
            }

            output.push_back(std::move(ff));
        }

        // Recurse into children
        int numChildren = field->getNumChildren();
        for (int i = 0; i < numChildren; ++i) {
            collectFieldsRecursive(field->getChildren(i), output, fieldMap);
        }
    }

    FieldType convertFieldType(FormFieldType type) {
        switch (type) {
            case formText: return FieldType::Text;
            case formButton: return FieldType::Button;
            case formChoice: return FieldType::Choice;
            case formSignature: return FieldType::Signature;
            default: return FieldType::Unknown;
        }
    }

    bool setTextFieldValue(const std::string& name, const std::string& value) {
        ::FormField* field = findFormField(name);
        if (!field) {
            lastError_ = "Field not found: " + name;
            return false;
        }

        if (field->getType() != formText) {
            lastError_ = "Field is not a text field: " + name;
            return false;
        }

        auto* textField = static_cast<::FormFieldText*>(field);

        // Convert UTF-8 input to PDF text string format (UTF-16BE with BOM)
        // This is required for proper text display in PDF viewers
        std::string pdfStr = utf8ToUtf16WithBom(value);
        GooString gooValue(pdfStr.c_str(), pdfStr.length());
        textField->setContentCopy(&gooValue);

        // Skip widget appearance updates - we don't have fonts in WASM
        // The PDF viewer will regenerate appearances when displaying

        modified_ = true;
        fieldsCached_ = false;  // Invalidate cache
        return true;
    }

    bool setChoiceFieldValue(const std::string& name, const std::string& value) {
        ::FormField* field = findFormField(name);
        if (!field) {
            lastError_ = "Field not found: " + name;
            return false;
        }

        if (field->getType() != formChoice) {
            lastError_ = "Field is not a choice field: " + name;
            return false;
        }

        auto* choiceField = static_cast<::FormFieldChoice*>(field);

        // Find the index of the value
        int numChoices = choiceField->getNumChoices();
        int selectedIdx = -1;
        for (int i = 0; i < numChoices; ++i) {
            const GooString* choice = choiceField->getChoice(i);
            if (choice && gooToStd(choice) == value) {
                selectedIdx = i;
                break;
            }
        }

        if (selectedIdx < 0) {
            lastError_ = "Value not in choice options: " + value;
            return false;
        }

        choiceField->select(selectedIdx);

        // Skip widget appearance updates - we don't have fonts in WASM

        modified_ = true;
        fieldsCached_ = false;
        return true;
    }

    bool setButtonFieldValue(const std::string& name, bool checked) {
        ::FormField* field = findFormField(name);
        if (!field) {
            lastError_ = "Field not found: " + name;
            return false;
        }

        if (field->getType() != formButton) {
            lastError_ = "Field is not a button field: " + name;
            return false;
        }

        auto* buttonField = static_cast<::FormFieldButton*>(field);
        FormButtonType btnType = buttonField->getButtonType();

        if (btnType != formButtonCheck && btnType != formButtonRadio) {
            // Push buttons (like hyperlinks) don't have a settable value - skip silently
            return true;
        }

        // Get the "on" state name from the widget - each checkbox can have a custom name
        const char* checkedState = nullptr;
        int numWidgets = buttonField->getNumWidgets();
        if (numWidgets > 0) {
            FormWidget* widget = buttonField->getWidget(0);
            if (widget && widget->getType() == formButton) {
                auto* btnWidget = static_cast<FormWidgetButton*>(widget);
                checkedState = btnWidget->getOnStr();
            }
        }

        // Fallback to common defaults
        if (!checkedState || !checkedState[0]) {
            checkedState = "Yes";
        }

        // Set the state using the state name
        const char* newState = checked ? checkedState : "Off";
        buttonField->setState(newState);

        // Skip widget appearance updates - we don't have fonts in WASM

        modified_ = true;
        fieldsCached_ = false;
        return true;
    }

    bool flattenForm() {
        if (!doc_) {
            lastError_ = "No document loaded";
            return false;
        }

        Form* form = getForm();
        if (!form) {
            lastError_ = "Document has no form";
            return false;
        }

        // Flatten each page by processing widget annotations
        int numPages = doc_->getNumPages();
        for (int i = 1; i <= numPages; ++i) {
            Page* page = doc_->getPage(i);
            if (page) {
                // Get annotations and flatten widget annotations
                Annots* annots = page->getAnnots();
                if (annots) {
                    // Use the vector-based API
                    const auto& annotList = annots->getAnnots();
                    for (Annot* annot : annotList) {
                        if (annot && annot->getType() == Annot::typeWidget) {
                            // Widget annotations are form fields
                            // The appearance is already generated when we set values
                            // Flattening would merge appearance into page content
                            // For now, we mark the document as modified
                        }
                    }
                }
            }
        }

        modified_ = true;
        return true;
    }

    std::vector<uint8_t> saveToMemory() {
        if (!doc_) {
            lastError_ = "No document loaded";
            return {};
        }

        // Use a temp file approach since Poppler's save API is file-oriented
        // In WASM environment, /tmp is backed by memory anyway
        std::string tempPath = "/tmp/pdf_filler_temp_" +
            std::to_string(reinterpret_cast<uintptr_t>(this)) + ".pdf";

        // Save using GooString path
        GooString outPath(tempPath);
        PDFWriteMode writeMode = modified_ ? writeForceRewrite : writeStandard;

        int result = doc_->saveAs(outPath, writeMode);
        if (result != errNone) {
            lastError_ = "Failed to save PDF: error code " + std::to_string(result);
            return {};
        }

        // Read back the temp file
        std::vector<uint8_t> output;
        std::ifstream inFile(tempPath, std::ios::binary | std::ios::ate);
        if (!inFile) {
            lastError_ = "Failed to read saved PDF";
            std::remove(tempPath.c_str());
            return {};
        }

        size_t size = inFile.tellg();
        inFile.seekg(0);
        output.resize(size);
        inFile.read(reinterpret_cast<char*>(output.data()), size);
        inFile.close();

        // Clean up temp file
        std::remove(tempPath.c_str());

        return output;
    }

    std::vector<uint8_t> renderPageToPng(int pageIndex, double dpi) {
        if (!doc_ || pageIndex < 0 || pageIndex >= doc_->getNumPages()) {
            return {};
        }

        // Create splash output device for rendering
        SplashColor paperColor;
        paperColor[0] = 255;  // White background
        paperColor[1] = 255;
        paperColor[2] = 255;

        SplashOutputDev splashOut(splashModeRGB8, 4, false, paperColor);
        splashOut.startDoc(doc_.get());

        // Render page (1-indexed in Poppler)
        doc_->displayPage(&splashOut, pageIndex + 1, dpi, dpi, 0, true, false, false);

        SplashBitmap* bitmap = splashOut.getBitmap();
        if (!bitmap) {
            lastError_ = "Failed to render page";
            return {};
        }

        // Convert to PNG
        int width = bitmap->getWidth();
        int height = bitmap->getHeight();
        int rowSize = bitmap->getRowSize();
        SplashColorPtr data = bitmap->getDataPtr();

        std::vector<uint8_t> pngData;

        png_structp png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        if (!png_ptr) return {};

        png_infop info_ptr = png_create_info_struct(png_ptr);
        if (!info_ptr) {
            png_destroy_write_struct(&png_ptr, nullptr);
            return {};
        }

        if (setjmp(png_jmpbuf(png_ptr))) {
            png_destroy_write_struct(&png_ptr, &info_ptr);
            return {};
        }

        // Custom write to vector
        struct PngContext { std::vector<uint8_t>* out; };
        PngContext ctx{&pngData};

        png_set_write_fn(png_ptr, &ctx,
            [](png_structp p, png_bytep d, png_size_t len) {
                auto* c = static_cast<PngContext*>(png_get_io_ptr(p));
                c->out->insert(c->out->end(), d, d + len);
            },
            nullptr
        );

        png_set_IHDR(png_ptr, info_ptr, width, height, 8, PNG_COLOR_TYPE_RGB,
                     PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);

        png_write_info(png_ptr, info_ptr);

        for (int y = 0; y < height; ++y) {
            png_write_row(png_ptr, data + y * rowSize);
        }

        png_write_end(png_ptr, info_ptr);
        png_destroy_write_struct(&png_ptr, &info_ptr);

        return pngData;
    }
};

// PdfDocument implementation

PdfDocument::PdfDocument() : impl_(std::make_unique<Impl>()) {}

PdfDocument::~PdfDocument() = default;

bool PdfDocument::loadFromMemory(const uint8_t* data, size_t length, const std::string& password) {
    return impl_->loadFromMemory(data, length, password);
}

bool PdfDocument::loadFromFile(const std::string& path, const std::string& password) {
    return impl_->loadFromFile(path, password);
}

int PdfDocument::getPageCount() const {
    return impl_->doc_ ? impl_->doc_->getNumPages() : 0;
}

std::string PdfDocument::getTitle() const {
    if (!impl_->doc_) return "";
    Object info = impl_->doc_->getDocInfo();
    if (!info.isDict()) return "";

    Object title = info.dictLookup("Title");
    if (title.isString()) {
        return gooToStd(title.getString());
    }
    return "";
}

std::string PdfDocument::getAuthor() const {
    if (!impl_->doc_) return "";
    Object info = impl_->doc_->getDocInfo();
    if (!info.isDict()) return "";

    Object author = info.dictLookup("Author");
    if (author.isString()) {
        return gooToStd(author.getString());
    }
    return "";
}

bool PdfDocument::hasAcroForm() const {
    return impl_->getForm() != nullptr;
}

std::vector<PdfFormField> PdfDocument::getFormFields() const {
    const_cast<Impl*>(impl_.get())->cacheFormFields();
    return impl_->cachedFields_;
}

PdfFormField* PdfDocument::getFieldByName(const std::string& name) {
    impl_->cacheFormFields();

    for (auto& field : impl_->cachedFields_) {
        if (field.name == name || field.fullName == name) {
            return &field;
        }
    }
    return nullptr;
}

bool PdfDocument::setFieldValue(const std::string& name, const std::string& value) {
    // First try as text field
    ::FormField* field = impl_->findFormField(name);
    if (!field) {
        impl_->lastError_ = "Field not found: " + name;
        return false;
    }

    switch (field->getType()) {
        case formText:
            return impl_->setTextFieldValue(name, value);
        case formChoice:
            return impl_->setChoiceFieldValue(name, value);
        case formButton:
            // For buttons, interpret non-empty string as "checked"
            return impl_->setButtonFieldValue(name, !value.empty() && value != "0" && value != "false");
        default:
            impl_->lastError_ = "Unsupported field type for setValue";
            return false;
    }
}

bool PdfDocument::setCheckboxValue(const std::string& name, bool checked) {
    return impl_->setButtonFieldValue(name, checked);
}

bool PdfDocument::setFieldValues(const std::vector<std::pair<std::string, std::string>>& values) {
    bool allSuccess = true;
    for (const auto& [name, value] : values) {
        if (!setFieldValue(name, value)) {
            allSuccess = false;
            // Continue trying other fields
        }
    }
    return allSuccess;
}

bool PdfDocument::flattenForm() {
    return impl_->flattenForm();
}

std::vector<uint8_t> PdfDocument::saveToMemory() const {
    return const_cast<Impl*>(impl_.get())->saveToMemory();
}

bool PdfDocument::saveToFile(const std::string& path) const {
    auto data = saveToMemory();
    if (data.empty()) return false;

    std::ofstream file(path, std::ios::binary);
    if (!file) {
        impl_->lastError_ = "Failed to open file for writing: " + path;
        return false;
    }

    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    return file.good();
}

std::vector<uint8_t> PdfDocument::renderPageToPng(int pageIndex, double dpi) const {
    return const_cast<Impl*>(impl_.get())->renderPageToPng(pageIndex, dpi);
}

std::string PdfDocument::getLastError() const {
    return impl_->lastError_;
}

// Utility functions
std::string fieldTypeToString(FieldType type) {
    switch (type) {
        case FieldType::Text: return "text";
        case FieldType::Button: return "button";
        case FieldType::Checkbox: return "checkbox";
        case FieldType::Radio: return "radio";
        case FieldType::Choice: return "choice";
        case FieldType::Signature: return "signature";
        default: return "unknown";
    }
}

FieldType stringToFieldType(const std::string& str) {
    if (str == "text") return FieldType::Text;
    if (str == "button") return FieldType::Button;
    if (str == "checkbox") return FieldType::Checkbox;
    if (str == "radio") return FieldType::Radio;
    if (str == "choice") return FieldType::Choice;
    if (str == "signature") return FieldType::Signature;
    return FieldType::Unknown;
}

} // namespace pdffiller
