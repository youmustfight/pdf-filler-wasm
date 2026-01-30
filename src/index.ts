/**
 * PDF Form Filler - TypeScript wrapper for Poppler WASM module
 */

import type { PdfFillerModule, PdfFillerInstance, FormField } from './types';

// Dynamic import for the WASM module
let modulePromise: Promise<PdfFillerModule> | null = null;

// Declare the global that Emscripten creates
declare global {
  var createPdfFillerModule: ((config?: object) => Promise<PdfFillerModule>) | undefined;
}

/**
 * Initialize the WASM module. Call this before using any other functions.
 * The module is cached after first initialization.
 */
export async function initPdfFiller(): Promise<PdfFillerModule> {
  if (modulePromise) {
    return modulePromise;
  }

  modulePromise = (async () => {
    // Dynamic import to support both Node.js and browser
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    const imported: any = await import('./pdf-filler.js');

    // Handle different module formats:
    // - CommonJS/Node: module.exports = createPdfFillerModule
    // - Browser: sets global createPdfFillerModule
    let createModule =
      imported.default ||                           // ES module default export
      imported.createPdfFillerModule ||             // Named export
      (typeof imported === 'function' ? imported : null) ||  // Direct function (CommonJS)
      (globalThis as any).createPdfFillerModule;    // Global (browser fallback)

    if (typeof createModule !== 'function') {
      throw new Error('Failed to load WASM module: createPdfFillerModule not found');
    }

    const Module = await createModule();
    return Module as PdfFillerModule;
  })();

  return modulePromise;
}

/**
 * High-level API for working with PDF forms
 */
export class PdfForm {
  private module: PdfFillerModule;
  private instance: PdfFillerInstance;
  private _loaded = false;

  private constructor(module: PdfFillerModule) {
    this.module = module;
    this.instance = new module.PdfFiller();
  }

  /**
   * Create a new PdfForm instance and load a PDF from an ArrayBuffer
   */
  static async fromArrayBuffer(
    data: ArrayBuffer,
    password?: string
  ): Promise<PdfForm> {
    const module = await initPdfFiller();
    const form = new PdfForm(module);

    const success = form.instance.loadFromArrayBuffer(data, password ?? '');
    if (!success) {
      const error = form.instance.getLastError();
      throw new Error(`Failed to load PDF: ${error}`);
    }

    form._loaded = true;
    return form;
  }

  /**
   * Create a new PdfForm instance and load a PDF from a Uint8Array
   */
  static async fromUint8Array(
    data: Uint8Array,
    password?: string
  ): Promise<PdfForm> {
    return PdfForm.fromArrayBuffer(data.buffer as ArrayBuffer, password);
  }

  /**
   * Create a new PdfForm instance and load a PDF from the virtual filesystem
   * (useful when running in Node.js with files mounted to Emscripten FS)
   */
  static async fromPath(path: string, password?: string): Promise<PdfForm> {
    const module = await initPdfFiller();
    const form = new PdfForm(module);

    const success = form.instance.loadFromPath(path, password ?? '');
    if (!success) {
      const error = form.instance.getLastError();
      throw new Error(`Failed to load PDF from path: ${error}`);
    }

    form._loaded = true;
    return form;
  }

  /**
   * Get the number of pages in the document
   */
  get pageCount(): number {
    this.ensureLoaded();
    return this.instance.getPageCount();
  }

  /**
   * Get the document title
   */
  get title(): string {
    this.ensureLoaded();
    return this.instance.getTitle();
  }

  /**
   * Get the document author
   */
  get author(): string {
    this.ensureLoaded();
    return this.instance.getAuthor();
  }

  /**
   * Check if the document has an AcroForm (fillable form)
   */
  get hasForm(): boolean {
    this.ensureLoaded();
    return this.instance.hasAcroForm();
  }

  /**
   * Get all form fields in the document
   */
  getFields(): FormField[] {
    this.ensureLoaded();
    return this.instance.getFormFields();
  }

  /**
   * Get a form field by name
   */
  getField(name: string): FormField | null {
    this.ensureLoaded();
    return this.instance.getFieldByName(name);
  }

  /**
   * Set a text field value
   */
  setField(name: string, value: string): void {
    this.ensureLoaded();
    const success = this.instance.setFieldValue(name, value);
    if (!success) {
      const error = this.instance.getLastError();
      throw new Error(`Failed to set field "${name}": ${error}`);
    }
  }

  /**
   * Set a checkbox field value
   */
  setCheckbox(name: string, checked: boolean): void {
    this.ensureLoaded();
    const success = this.instance.setCheckboxValue(name, checked);
    if (!success) {
      const error = this.instance.getLastError();
      throw new Error(`Failed to set checkbox "${name}": ${error}`);
    }
  }

  /**
   * Set multiple field values at once
   */
  setFields(values: Record<string, string>): void {
    this.ensureLoaded();
    const success = this.instance.setFieldValues(values);
    if (!success) {
      const error = this.instance.getLastError();
      throw new Error(`Failed to set fields: ${error}`);
    }
  }

  /**
   * Flatten the form (make fields non-editable, embed into page content)
   */
  flatten(): void {
    this.ensureLoaded();
    const success = this.instance.flattenForm();
    if (!success) {
      const error = this.instance.getLastError();
      throw new Error(`Failed to flatten form: ${error}`);
    }
  }

  /**
   * Save the PDF to an ArrayBuffer
   */
  save(): ArrayBuffer {
    this.ensureLoaded();
    const result = this.instance.saveToArrayBuffer();
    if (result === null) {
      const error = this.instance.getLastError();
      throw new Error(`Failed to save PDF: ${error}`);
    }
    return result;
  }

  /**
   * Save the PDF to a Uint8Array
   */
  saveAsUint8Array(): Uint8Array {
    return new Uint8Array(this.save());
  }

  /**
   * Render a page to PNG image data
   */
  renderPage(pageIndex: number, dpi = 150): Uint8Array {
    this.ensureLoaded();
    if (pageIndex < 0 || pageIndex >= this.pageCount) {
      throw new Error(`Page index ${pageIndex} out of range (0-${this.pageCount - 1})`);
    }

    const result = this.instance.renderPageToPng(pageIndex, dpi);
    if (result === null) {
      const error = this.instance.getLastError();
      throw new Error(`Failed to render page: ${error}`);
    }
    return result;
  }

  /**
   * Access the Emscripten filesystem for loading/saving files
   */
  get FS() {
    return this.module.FS;
  }

  /**
   * Get the last error message from the native code
   */
  get lastError(): string {
    return this.instance.getLastError();
  }

  private ensureLoaded(): void {
    if (!this._loaded) {
      throw new Error('PDF not loaded. Use PdfForm.fromArrayBuffer() or similar.');
    }
  }
}

// Re-export types
export type { FormField, FieldType } from './types';

// Default export for convenience
export default PdfForm;
