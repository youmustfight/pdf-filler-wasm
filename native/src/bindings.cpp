// Emscripten bindings for pdf-filler
#include "pdf-filler.h"

#include <emscripten/bind.h>
#include <emscripten/val.h>

using namespace emscripten;
using namespace pdffiller;

// JavaScript-friendly wrapper
class PdfFillerJS {
public:
    PdfFillerJS() : doc_(std::make_unique<PdfDocument>()) {}

    bool loadFromArrayBuffer(const val& arrayBuffer, const std::string& password = "") {
        // Get the ArrayBuffer data
        unsigned int length = arrayBuffer["byteLength"].as<unsigned int>();
        std::vector<uint8_t> data(length);

        val uint8Array = val::global("Uint8Array").new_(arrayBuffer);
        for (unsigned int i = 0; i < length; ++i) {
            data[i] = uint8Array[i].as<uint8_t>();
        }

        return doc_->loadFromMemory(data.data(), data.size(), password);
    }

    bool loadFromPath(const std::string& path, const std::string& password = "") {
        return doc_->loadFromFile(path, password);
    }

    int getPageCount() const {
        return doc_->getPageCount();
    }

    std::string getTitle() const {
        return doc_->getTitle();
    }

    std::string getAuthor() const {
        return doc_->getAuthor();
    }

    bool hasAcroForm() const {
        return doc_->hasAcroForm();
    }

    val getFormFields() const {
        auto fields = doc_->getFormFields();
        val result = val::array();

        for (size_t i = 0; i < fields.size(); ++i) {
            const auto& f = fields[i];
            val field = val::object();

            field.set("name", f.name);
            field.set("fullName", f.fullName);
            field.set("value", f.value);
            field.set("defaultValue", f.defaultValue);
            field.set("type", fieldTypeToString(f.type));
            field.set("readOnly", f.readOnly);
            field.set("required", f.required);
            field.set("pageIndex", f.pageIndex);
            field.set("x", f.x);
            field.set("y", f.y);
            field.set("width", f.width);
            field.set("height", f.height);
            field.set("exportValue", f.exportValue);
            field.set("isChecked", f.isChecked);

            val options = val::array();
            for (size_t j = 0; j < f.options.size(); ++j) {
                options.set(j, f.options[j]);
            }
            field.set("options", options);

            result.set(i, field);
        }

        return result;
    }

    val getFieldByName(const std::string& name) const {
        auto* field = const_cast<PdfDocument*>(doc_.get())->getFieldByName(name);
        if (!field) {
            return val::null();
        }

        val result = val::object();
        result.set("name", field->name);
        result.set("fullName", field->fullName);
        result.set("value", field->value);
        result.set("defaultValue", field->defaultValue);
        result.set("type", fieldTypeToString(field->type));
        result.set("readOnly", field->readOnly);
        result.set("required", field->required);
        result.set("pageIndex", field->pageIndex);
        result.set("x", field->x);
        result.set("y", field->y);
        result.set("width", field->width);
        result.set("height", field->height);
        result.set("exportValue", field->exportValue);
        result.set("isChecked", field->isChecked);

        val options = val::array();
        for (size_t i = 0; i < field->options.size(); ++i) {
            options.set(i, field->options[i]);
        }
        result.set("options", options);

        return result;
    }

    bool setFieldValue(const std::string& name, const std::string& value) {
        return doc_->setFieldValue(name, value);
    }

    bool setCheckboxValue(const std::string& name, bool checked) {
        return doc_->setCheckboxValue(name, checked);
    }

    bool setFieldValues(const val& values) {
        std::vector<std::pair<std::string, std::string>> pairs;

        val keys = val::global("Object").call<val>("keys", values);
        unsigned int length = keys["length"].as<unsigned int>();

        for (unsigned int i = 0; i < length; ++i) {
            std::string key = keys[i].as<std::string>();
            std::string value = values[key].as<std::string>();
            pairs.emplace_back(key, value);
        }

        return doc_->setFieldValues(pairs);
    }

    bool flattenForm() {
        return doc_->flattenForm();
    }

    val saveToArrayBuffer() const {
        auto data = doc_->saveToMemory();
        if (data.empty()) {
            return val::null();
        }

        val uint8Array = val::global("Uint8Array").new_(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            uint8Array.set(i, data[i]);
        }

        return uint8Array["buffer"];
    }

    bool saveToPath(const std::string& path) const {
        return doc_->saveToFile(path);
    }

    val renderPageToPng(int pageIndex, double dpi = 150.0) const {
        auto data = doc_->renderPageToPng(pageIndex, dpi);
        if (data.empty()) {
            return val::null();
        }

        val uint8Array = val::global("Uint8Array").new_(data.size());
        for (size_t i = 0; i < data.size(); ++i) {
            uint8Array.set(i, data[i]);
        }

        return uint8Array;
    }

    std::string getLastError() const {
        return doc_->getLastError();
    }

private:
    std::unique_ptr<PdfDocument> doc_;
};

// Emscripten bindings
EMSCRIPTEN_BINDINGS(pdf_filler) {
    class_<PdfFillerJS>("PdfFiller")
        .constructor<>()
        .function("loadFromArrayBuffer", &PdfFillerJS::loadFromArrayBuffer)
        .function("loadFromPath", &PdfFillerJS::loadFromPath)
        .function("getPageCount", &PdfFillerJS::getPageCount)
        .function("getTitle", &PdfFillerJS::getTitle)
        .function("getAuthor", &PdfFillerJS::getAuthor)
        .function("hasAcroForm", &PdfFillerJS::hasAcroForm)
        .function("getFormFields", &PdfFillerJS::getFormFields)
        .function("getFieldByName", &PdfFillerJS::getFieldByName)
        .function("setFieldValue", &PdfFillerJS::setFieldValue)
        .function("setCheckboxValue", &PdfFillerJS::setCheckboxValue)
        .function("setFieldValues", &PdfFillerJS::setFieldValues)
        .function("flattenForm", &PdfFillerJS::flattenForm)
        .function("saveToArrayBuffer", &PdfFillerJS::saveToArrayBuffer)
        .function("saveToPath", &PdfFillerJS::saveToPath)
        .function("renderPageToPng", &PdfFillerJS::renderPageToPng)
        .function("getLastError", &PdfFillerJS::getLastError);
}
