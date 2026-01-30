import { describe, it, expect, beforeAll } from 'vitest';
import { PdfForm, initPdfFiller } from '../src/index';
import * as fs from 'fs';
import * as path from 'path';

// Skip tests if WASM module not built
const wasmPath = path.join(__dirname, '../dist/pdf-filler.wasm');
const wasmExists = fs.existsSync(wasmPath);

// Test PDF path
const testPdfPath = path.join(__dirname, 'hc001.pdf');
const testPdfExists = fs.existsSync(testPdfPath);

describe.skipIf(!wasmExists)('PdfForm', () => {
  beforeAll(async () => {
    await initPdfFiller();
  });

  describe('loading', () => {
    it.skipIf(!testPdfExists)('should load HC-001 PDF from ArrayBuffer', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      expect(form.pageCount).toBe(8);
      expect(form.hasForm).toBe(true);
    });

    it('should throw on invalid PDF data', async () => {
      const invalidData = new Uint8Array([0, 1, 2, 3]);
      await expect(PdfForm.fromUint8Array(invalidData)).rejects.toThrow();
    });
  });

  describe.skipIf(!testPdfExists)('form fields', () => {
    it('should list form fields', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      expect(fields).toBeInstanceOf(Array);
      expect(fields.length).toBeGreaterThan(0);

      // Log field names for debugging
      console.log(`Found ${fields.length} fields:`);
      for (const field of fields.slice(0, 20)) {
        console.log(`  - ${field.fullName} (${field.type})`);
      }
    });

    it('should find text fields', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      const textFields = fields.filter(f => f.type === 'text');

      expect(textFields.length).toBeGreaterThan(0);
    });

    it('should find checkbox fields', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      const checkboxFields = fields.filter(f => f.type === 'checkbox');

      expect(checkboxFields.length).toBeGreaterThan(0);
    });
  });

  describe.skipIf(!testPdfExists)('field modification', () => {
    it('should set a text field without throwing', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      const textField = fields.find(f => f.type === 'text');
      expect(textField).toBeDefined();

      // Should not throw
      expect(() => form.setField(textField!.fullName, 'Test Value')).not.toThrow();
    });

    it('should set a checkbox without throwing', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      const checkbox = fields.find(f => f.type === 'checkbox');
      expect(checkbox).toBeDefined();

      // Should not throw
      expect(() => form.setCheckbox(checkbox!.fullName, true)).not.toThrow();
      expect(() => form.setCheckbox(checkbox!.fullName, false)).not.toThrow();
    });
  });

  describe.skipIf(!testPdfExists)('saving', () => {
    it('should save unmodified PDF', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const saved = form.saveAsUint8Array();
      expect(saved.length).toBeGreaterThan(0);

      // Verify it's still a valid PDF (starts with %PDF-)
      const header = String.fromCharCode(...saved.slice(0, 5));
      expect(header).toBe('%PDF-');
    });

    it('should save PDF after setting text field', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      const textField = fields.find(f => f.type === 'text');
      expect(textField).toBeDefined();

      form.setField(textField!.fullName, 'Test Value');

      const saved = form.saveAsUint8Array();
      expect(saved.length).toBeGreaterThan(0);

      // Verify it's still a valid PDF
      const header = String.fromCharCode(...saved.slice(0, 5));
      expect(header).toBe('%PDF-');
    });

    it('should save PDF after setting checkbox', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const fields = form.getFields();
      const checkbox = fields.find(f => f.type === 'checkbox');
      expect(checkbox).toBeDefined();

      form.setCheckbox(checkbox!.fullName, true);

      const saved = form.saveAsUint8Array();
      expect(saved.length).toBeGreaterThan(0);

      // Verify it's still a valid PDF
      const header = String.fromCharCode(...saved.slice(0, 5));
      expect(header).toBe('%PDF-');
    });
  });

  describe.skipIf(!testPdfExists)('rendering', () => {
    it('should render first page to PNG', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const png = form.renderPage(0, 72);

      expect(png).toBeInstanceOf(Uint8Array);
      expect(png.length).toBeGreaterThan(0);

      // Check PNG magic bytes
      expect(png[0]).toBe(0x89);
      expect(png[1]).toBe(0x50); // P
      expect(png[2]).toBe(0x4e); // N
      expect(png[3]).toBe(0x47); // G
    });

    it('should render at different DPI settings', async () => {
      const data = fs.readFileSync(testPdfPath);
      const form = await PdfForm.fromUint8Array(data);

      const png72 = form.renderPage(0, 72);
      const png150 = form.renderPage(0, 150);

      // Higher DPI should produce larger image
      expect(png150.length).toBeGreaterThan(png72.length);
    });
  });
});
