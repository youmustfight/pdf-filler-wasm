// Configure WASM location before importing library
window.Module = {
  locateFile: (path) => {
    if (path.endsWith('.wasm')) {
      // Resolve relative to the page, not the script
      return new URL('./assets/pdf-filler.wasm', window.location.href).href;
    }
    return path;
  }
};

// Dynamic import so Module config is set first
const { PdfForm, initPdfFiller } = await import('pdf-filler-wasm');

let currentForm = null;

const statusEl = document.getElementById('status');
const dropZone = document.getElementById('dropZone');
const fileInput = document.getElementById('fileInput');
const fieldList = document.getElementById('fieldList');
const saveBtn = document.getElementById('saveBtn');
const preview = document.getElementById('preview');

function setStatus(message, type = 'loading') {
  statusEl.textContent = message;
  statusEl.className = `status ${type}`;
}

// Initialize WASM
try {
  await initPdfFiller();
  setStatus('Ready! Drop a PDF file to begin.', 'success');
} catch (err) {
  setStatus(`Failed to load WASM: ${err.message}`, 'error');
}

// File drop handling
dropZone.addEventListener('click', () => fileInput.click());
dropZone.addEventListener('dragover', (e) => {
  e.preventDefault();
  dropZone.classList.add('dragover');
});
dropZone.addEventListener('dragleave', () => {
  dropZone.classList.remove('dragover');
});
dropZone.addEventListener('drop', async (e) => {
  e.preventDefault();
  dropZone.classList.remove('dragover');
  const file = e.dataTransfer.files[0];
  if (file) await loadFile(file);
});
fileInput.addEventListener('change', async (e) => {
  const file = e.target.files[0];
  if (file) await loadFile(file);
});

async function loadFile(file) {
  setStatus('Loading PDF...', 'loading');
  try {
    const buffer = await file.arrayBuffer();
    currentForm = await PdfForm.fromArrayBuffer(buffer);

    setStatus(`Loaded: ${currentForm.pageCount} pages`, 'success');
    renderFields();
    renderPreview();
    saveBtn.disabled = false;
  } catch (err) {
    setStatus(`Error: ${err.message}`, 'error');
  }
}

// Helper to escape HTML attributes
function escapeAttr(str) {
  return String(str || '').replace(/&/g, '&amp;').replace(/"/g, '&quot;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
}

function renderFields() {
  const fields = currentForm.getFields();
  if (fields.length === 0) {
    fieldList.innerHTML = '<p>No fillable fields found</p>';
    return;
  }

  // Filter out button fields (push buttons like links) - they're not editable
  const editableFields = fields.filter(f => f.type !== 'button');

  fieldList.innerHTML = editableFields.map((field, i) => {
    // Use fullName for lookups (fully qualified name)
    const fieldId = escapeAttr(field.fullName || field.name);
    const displayName = field.fullName || field.name;
    const fieldValue = escapeAttr(field.value || '');

    return `
      <div class="field-item">
        <div class="field-name">${displayName}</div>
        <div class="field-type">${field.type}${field.required ? ' (required)' : ''}</div>
        ${field.type === 'checkbox'
          ? `<input type="checkbox" class="field-input" data-name="${fieldId}" ${field.isChecked ? 'checked' : ''}>`
          : field.type === 'choice'
          ? `<select class="field-input" data-name="${fieldId}">
              ${field.options.map(opt => `<option ${opt === field.value ? 'selected' : ''}>${escapeAttr(opt)}</option>`).join('')}
            </select>`
          : `<input type="text" class="field-input" data-name="${fieldId}" value="${fieldValue}">`
        }
      </div>
    `;
  }).join('');
}

function renderPreview() {
  try {
    const png = currentForm.renderPage(0, 96);
    const blob = new Blob([png], { type: 'image/png' });
    const url = URL.createObjectURL(blob);
    const img = new Image();
    img.onload = () => {
      const ctx = preview.getContext('2d');
      preview.width = img.width;
      preview.height = img.height;
      ctx.drawImage(img, 0, 0);
      URL.revokeObjectURL(url);
    };
    img.src = url;
  } catch (err) {
    console.warn('Preview failed:', err);
  }
}

saveBtn.addEventListener('click', () => {
  // Collect field values - wrap each in try-catch to handle errors
  const inputs = fieldList.querySelectorAll('.field-input');
  let successCount = 0;
  let errorCount = 0;

  for (const input of inputs) {
    const name = input.dataset.name;
    try {
      if (input.type === 'checkbox') {
        currentForm.setCheckbox(name, input.checked);
        successCount++;
      } else if (input.value) {
        // Only set non-empty text values
        currentForm.setField(name, input.value);
        successCount++;
      }
    } catch (err) {
      console.warn(`Failed to set field "${name}":`, err.message);
      errorCount++;
    }
  }

  console.log(`Set ${successCount} fields, ${errorCount} errors`);

  // Download
  let data;
  try {
    data = currentForm.saveAsUint8Array();
  } catch (err) {
    setStatus(`Save failed: ${err.message}`, 'error');
    return;
  }
  const blob = new Blob([data], { type: 'application/pdf' });
  const url = URL.createObjectURL(blob);
  const a = document.createElement('a');
  a.href = url;
  a.download = 'filled-form.pdf';
  a.click();
  URL.revokeObjectURL(url);
});
