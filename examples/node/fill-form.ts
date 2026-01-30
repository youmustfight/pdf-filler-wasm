/**
 * Example: Fill a PDF form in Node.js
 */
import * as fs from 'fs';
import * as path from 'path';
import { PdfForm } from '../../src/index';

async function main() {
  // Load the PDF
  const inputPath = process.argv[2] || 'input.pdf';
  const outputPath = process.argv[3] || 'output.pdf';

  console.log(`Loading PDF: ${inputPath}`);
  const pdfData = fs.readFileSync(inputPath);
  const form = await PdfForm.fromUint8Array(pdfData);

  console.log(`Title: ${form.title || '(none)'}`);
  console.log(`Author: ${form.author || '(none)'}`);
  console.log(`Pages: ${form.pageCount}`);
  console.log(`Has form: ${form.hasForm}`);

  // List all form fields
  const fields = form.getFields();
  console.log(`\nForm fields (${fields.length}):`);
  for (const field of fields) {
    console.log(`  - ${field.fullName} (${field.type}): "${field.value}"`);
  }

  // Example: Fill in some fields
  // Modify this based on your actual PDF form field names
  const valuesToFill: Record<string, string> = {
    // 'FirstName': 'John',
    // 'LastName': 'Doe',
    // 'Email': 'john.doe@example.com',
  };

  if (Object.keys(valuesToFill).length > 0) {
    console.log('\nFilling fields...');
    form.setFields(valuesToFill);
  }

  // Save the result
  console.log(`\nSaving to: ${outputPath}`);
  const outputData = form.saveAsUint8Array();
  fs.writeFileSync(outputPath, outputData);

  console.log('Done!');
}

main().catch((err) => {
  console.error('Error:', err);
  process.exit(1);
});
