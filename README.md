# pdf-filler-wasm

A PDF form filling library using Poppler + Cairo compiled to WebAssembly. Works in Node.js and the browser.

## Features

- Fill PDF form fields (text, checkbox, radio buttons, dropdowns)
- Read form field values and metadata
- Render PDF pages to PNG images
- Works entirely client-side - no server required
- TypeScript support with full type definitions

## Installation

```bash
npm install pdf-filler-wasm
```

## Quick Start

### Node.js

```typescript
import { PdfForm } from 'pdf-filler-wasm';
import fs from 'fs';

// Load a PDF
const data = fs.readFileSync('form.pdf');
const form = await PdfForm.fromUint8Array(data);

// List all form fields
const fields = form.getFields();
console.log(`Found ${fields.length} fields`);

// Fill text fields
form.setField('name', 'John Doe');
form.setField('email', 'john@example.com');

// Set checkboxes
form.setCheckbox('agree_terms', true);

// Save the filled PDF
const output = form.saveAsUint8Array();
fs.writeFileSync('filled-form.pdf', output);
```

### Browser

```html
<script type="module">
  import { PdfForm, initPdfFiller } from 'pdf-filler-wasm';

  // Initialize the WASM module
  await initPdfFiller();

  // Load PDF from file input
  const fileInput = document.querySelector('input[type="file"]');
  fileInput.addEventListener('change', async (e) => {
    const file = e.target.files[0];
    const buffer = await file.arrayBuffer();
    const form = await PdfForm.fromArrayBuffer(buffer);

    // Fill fields
    form.setField('name', 'Jane Doe');

    // Download filled PDF
    const data = form.saveAsUint8Array();
    const blob = new Blob([data], { type: 'application/pdf' });
    const url = URL.createObjectURL(blob);

    const a = document.createElement('a');
    a.href = url;
    a.download = 'filled.pdf';
    a.click();
  });
</script>
```

## API Reference

### `initPdfFiller(): Promise<PdfFillerModule>`

Initialize the WASM module. Called automatically when using `PdfForm.fromArrayBuffer()`, but can be called explicitly to preload the module.

### `PdfForm`

#### Static Methods

- `PdfForm.fromArrayBuffer(data: ArrayBuffer, password?: string): Promise<PdfForm>` - Load a PDF from an ArrayBuffer
- `PdfForm.fromUint8Array(data: Uint8Array, password?: string): Promise<PdfForm>` - Load a PDF from a Uint8Array

#### Properties

- `pageCount: number` - Number of pages in the document
- `title: string` - Document title
- `author: string` - Document author
- `hasForm: boolean` - Whether the document has fillable form fields

#### Methods

- `getFields(): FormField[]` - Get all form fields
- `getField(name: string): FormField | null` - Get a field by name
- `setField(name: string, value: string): void` - Set a text field value
- `setCheckbox(name: string, checked: boolean): void` - Set a checkbox value
- `setFields(values: Record<string, string>): void` - Set multiple field values
- `flatten(): void` - Flatten the form (make fields non-editable)
- `save(): ArrayBuffer` - Save the PDF to an ArrayBuffer
- `saveAsUint8Array(): Uint8Array` - Save the PDF to a Uint8Array
- `renderPage(pageIndex: number, dpi?: number): Uint8Array` - Render a page to PNG

### `FormField`

```typescript
interface FormField {
  name: string;           // Field name
  fullName: string;       // Fully qualified name (for nested fields)
  type: FieldType;        // 'text' | 'checkbox' | 'radio' | 'button' | 'choice' | 'signature'
  value: string;          // Current value
  defaultValue: string;   // Default value
  isReadOnly: boolean;
  isRequired: boolean;
  isChecked: boolean;     // For checkbox/radio fields
  options: string[];      // For choice fields (dropdowns)
  pageIndex: number;      // Page the field appears on
}
```

## Building from Source

### Prerequisites

- Node.js 18+
- Docker (for building the WASM module)
- pnpm (recommended) or npm

### Build Steps

```bash
# Clone the repository
git clone https://github.com/youmustfight/pdf-filler-js.git
cd pdf-filler-js

# Install dependencies
pnpm install

# Build the Docker image (first time only)
pnpm build:docker

# Build native dependencies (first time only)
pnpm build:deps

# Build the WASM module and TypeScript
pnpm build

# Run tests
pnpm test

# Run the browser example
pnpm example
```

## How It Works

This library compiles [Poppler](https://poppler.freedesktop.org/) (a PDF rendering library) and [Cairo](https://cairographics.org/) (a 2D graphics library) to WebAssembly using [Emscripten](https://emscripten.org/). The native C++ code handles PDF parsing, form field manipulation, and rendering, while the TypeScript wrapper provides a clean JavaScript API.

### Dependencies

The WASM build includes:
- Poppler (PDF parsing and form handling)
- Cairo (2D rendering)
- FreeType (font rendering)
- libpng, libjpeg, libtiff, OpenJPEG (image codecs)
- zlib (compression)
- Pixman (pixel manipulation)

## Limitations

- Password-protected PDFs with encrypted form fields may not be fully supported
- Some advanced form features (JavaScript actions, XFA forms) are not supported
- The WASM bundle is approximately 3MB (can be gzipped to ~1MB)

## License

GPL-3.0 - This project uses Poppler which is licensed under GPL-2.0-or-later. See [LICENSE](LICENSE) for details.

## Contributing

Contributions are welcome! Please open an issue or submit a pull request.

## Acknowledgments

- [Poppler](https://poppler.freedesktop.org/) - The PDF rendering library
- [Cairo](https://cairographics.org/) - 2D graphics library
- [Emscripten](https://emscripten.org/) - The compiler toolchain for WebAssembly
