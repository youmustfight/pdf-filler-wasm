/**
 * Type definitions for the PDF Filler WASM module
 */

export type FieldType =
  | 'unknown'
  | 'text'
  | 'button'
  | 'checkbox'
  | 'radio'
  | 'choice'
  | 'signature';

export interface FormField {
  /** Field name */
  name: string;
  /** Fully qualified name (parent.child) */
  fullName: string;
  /** Current value */
  value: string;
  /** Default value */
  defaultValue: string;
  /** Field type */
  type: FieldType;
  /** Whether the field is read-only */
  readOnly: boolean;
  /** Whether the field is required */
  required: boolean;
  /** Page index (0-based) */
  pageIndex: number;
  /** X position in PDF points */
  x: number;
  /** Y position in PDF points */
  y: number;
  /** Width in PDF points */
  width: number;
  /** Height in PDF points */
  height: number;
  /** For choice fields: available options */
  options: string[];
  /** For checkboxes/radios: value when checked */
  exportValue: string;
  /** For checkboxes/radios: current state */
  isChecked: boolean;
}

/**
 * Low-level instance returned by the WASM module
 */
export interface PdfFillerInstance {
  loadFromArrayBuffer(data: ArrayBuffer, password: string): boolean;
  loadFromPath(path: string, password: string): boolean;
  getPageCount(): number;
  getTitle(): string;
  getAuthor(): string;
  hasAcroForm(): boolean;
  getFormFields(): FormField[];
  getFieldByName(name: string): FormField | null;
  setFieldValue(name: string, value: string): boolean;
  setCheckboxValue(name: string, checked: boolean): boolean;
  setFieldValues(values: Record<string, string>): boolean;
  flattenForm(): boolean;
  saveToArrayBuffer(): ArrayBuffer | null;
  saveToPath(path: string): boolean;
  renderPageToPng(pageIndex: number, dpi: number): Uint8Array | null;
  getLastError(): string;
}

/**
 * Emscripten FS interface (subset)
 */
export interface EmscriptenFS {
  writeFile(path: string, data: Uint8Array): void;
  readFile(path: string): Uint8Array;
  unlink(path: string): void;
  mkdir(path: string): void;
  rmdir(path: string): void;
  readdir(path: string): string[];
  stat(path: string): { size: number; mtime: Date };
}

/**
 * The WASM module interface
 */
export interface PdfFillerModule {
  PdfFiller: new () => PdfFillerInstance;
  FS: EmscriptenFS;
  HEAPU8: Uint8Array;
  ccall: (
    name: string,
    returnType: string,
    argTypes: string[],
    args: unknown[]
  ) => unknown;
  cwrap: (
    name: string,
    returnType: string,
    argTypes: string[]
  ) => (...args: unknown[]) => unknown;
}

/**
 * Module factory function
 */
export type CreatePdfFillerModule = () => Promise<PdfFillerModule>;
