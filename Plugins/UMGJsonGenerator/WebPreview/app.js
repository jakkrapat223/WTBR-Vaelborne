const SUPPORTED_TYPES = new Set([
  "CanvasPanel",
  "TextBlock",
  "Button",
  "Image",
  "Border",
  "VerticalBox",
  "HorizontalBox",
  "ProgressBar",
  "ScrollBox",
  "Overlay",
  "SizeBox",
  "Spacer"
]);

const VALID_JUSTIFY = new Set(["left", "center", "right"]);
const VALID_VERTICAL_ALIGN = new Set(["top", "center", "bottom"]);
// Must stay in sync with the C++ importer (UMGJsonImporter.cpp drawAs validation).
const VALID_DRAW_AS = new Set(["image", "box", "border", "roundedBox", "none"]);

const DEFAULT_RESOLUTION = { width: 1280, height: 720 };
const DEFAULT_SIZE = { width: 100, height: 30 };

const elements = {
  fileInput: document.getElementById("fileInput"),
  dropZone: document.getElementById("dropZone"),
  jsonInput: document.getElementById("jsonInput"),
  renderButton: document.getElementById("renderButton"),
  clearButton: document.getElementById("clearButton"),
  previewViewport: document.getElementById("previewViewport"),
  previewScaler: document.getElementById("previewScaler"),
  previewCanvas: document.getElementById("previewCanvas"),
  widgetNameLabel: document.getElementById("widgetNameLabel"),
  resolutionLabel: document.getElementById("resolutionLabel"),
  statsList: document.getElementById("statsList"),
  treePanel: document.getElementById("treePanel"),
  validationPanel: document.getElementById("validationPanel"),
  zoomButtons: Array.from(document.querySelectorAll(".zoom-button"))
};

let currentZoom = "fit";
let currentResolution = { ...DEFAULT_RESOLUTION };

elements.fileInput.addEventListener("change", async (event) => {
  const [file] = event.target.files;
  if (file) {
    elements.jsonInput.value = await file.text();
    renderFromInput();
  }
});

elements.dropZone.addEventListener("dragover", (event) => {
  event.preventDefault();
  elements.dropZone.classList.add("is-dragging");
});

elements.dropZone.addEventListener("dragleave", () => {
  elements.dropZone.classList.remove("is-dragging");
});

elements.dropZone.addEventListener("drop", async (event) => {
  event.preventDefault();
  elements.dropZone.classList.remove("is-dragging");
  const [file] = event.dataTransfer.files;
  if (file) {
    elements.jsonInput.value = await file.text();
    renderFromInput();
  }
});

elements.renderButton.addEventListener("click", renderFromInput);

elements.clearButton.addEventListener("click", () => {
  elements.fileInput.value = "";
  elements.jsonInput.value = "";
  resetView();
});

elements.zoomButtons.forEach((button) => {
  button.addEventListener("click", () => {
    currentZoom = button.dataset.zoom;
    updateZoomButtons();
    applyZoom();
  });
});

window.addEventListener("resize", () => {
  if (currentZoom === "fit") {
    applyZoom();
  }
});

resetView();

function renderFromInput() {
  const rawText = elements.jsonInput.value.trim();
  if (!rawText) {
    showValidation([{ level: "error", message: "No JSON input provided." }]);
    clearPreviewOnly();
    return;
  }

  const parseResult = parseJson(rawText);
  if (!parseResult.ok) {
    showValidation([{ level: "error", message: `Invalid JSON syntax: ${parseResult.error}` }]);
    clearPreviewOnly();
    setStats({
      widgetName: "-",
      resolution: "-",
      rootName: "-",
      widgetCount: 0,
      warnings: 0,
      skipped: 0,
      supported: 0,
      unsupported: 0
    });
    return;
  }

  const result = validateLayout(parseResult.value);
  showValidation(result.messages);
  setStats(result.stats);
  renderTree(result);

  if (result.blockingErrors.length > 0) {
    clearPreviewOnly("Validation failed before preview render.");
    return;
  }

  renderPreview(result);
}

function parseJson(rawText) {
  try {
    return { ok: true, value: JSON.parse(rawText) };
  } catch (error) {
    return { ok: false, error: error.message };
  }
}

function validateLayout(layout) {
  const messages = [];
  const blockingErrors = [];
  const widgetName = typeof layout.widgetName === "string" ? layout.widgetName : undefined;
  const resolution = normalizeResolution(layout.resolution);
  const root = layout.root && typeof layout.root === "object" ? { ...layout.root } : null;
  const widgets = Array.isArray(layout.widgets) ? layout.widgets : [];
  const nodeMap = new Map();
  const childrenByParent = new Map();
  const validWidgets = [];
  let skipped = 0;
  let unsupported = 0;

  if (widgetName === undefined) {
    pushMessage("error", "Missing required field: widgetName.");
  } else if (widgetName.trim() === "") {
    pushMessage("error", "Empty required field: widgetName.");
  }

  if (!root) {
    pushMessage("error", "Missing required field: root.");
  } else {
    root.type = stringOr(root.type, "");
    root.name = stringOr(root.name, "RootCanvas");
    if (root.type !== "CanvasPanel") {
      pushMessage("error", `Invalid root type '${root.type || "(empty)"}'. Root must be CanvasPanel.`);
    }
    if (!root.name.trim()) {
      pushMessage("error", "Root widget has an empty name.");
    }
  }

  if (blockingErrors.length > 0) {
    return buildResult();
  }

  nodeMap.set(root.name, {
    ...root,
    parent: "",
    children: [],
    warnings: []
  });
  childrenByParent.set(root.name, []);

  widgets.forEach((rawWidget, index) => {
    const widget = normalizeWidget(rawWidget, index);
    const label = widget.name || `widgets[${index}]`;

    if (!SUPPORTED_TYPES.has(widget.type)) {
      unsupported += 1;
      skipped += 1;
      pushMessage("warning", `${label}: unsupported widget type '${widget.type || "(empty)"}' - skipped.`);
      return;
    }

    if (!widget.name) {
      skipped += 1;
      pushMessage("warning", `widgets[${index}]: missing widget name - skipped.`);
      return;
    }

    if (nodeMap.has(widget.name)) {
      skipped += 1;
      pushMessage("warning", `Duplicate widget name '${widget.name}' - duplicate skipped.`);
      return;
    }

    const parentName = widget.parent || root.name;
    widget.parent = parentName;
    if (!parentName) {
      skipped += 1;
      pushMessage("warning", `${widget.name}: missing parent and no root fallback - skipped.`);
      return;
    }

    const parent = nodeMap.get(parentName);
    if (!parent) {
      skipped += 1;
      pushMessage("warning", `${widget.name}: parent '${parentName}' not found - skipped.`);
      return;
    }

    validateWidgetFields(widget, index, parent, messages);
    widget.children = [];
    widget.warnings = messagesForWidget(messages, widget.name);
    nodeMap.set(widget.name, widget);
    validWidgets.push(widget);
    if (!childrenByParent.has(parentName)) {
      childrenByParent.set(parentName, []);
    }
    childrenByParent.get(parentName).push(widget);
  });

  validWidgets.forEach((widget) => {
    widget.children = childrenByParent.get(widget.name) || [];
  });
  nodeMap.get(root.name).children = childrenByParent.get(root.name) || [];

  if (messages.length === 0) {
    pushMessage("ok", "Validation passed.");
  }

  return buildResult();

  function pushMessage(level, message) {
    messages.push({ level, message });
    if (level === "error") {
      blockingErrors.push(message);
    }
  }

  function buildResult() {
    const warningCount = messages.filter((message) => message.level === "warning").length;
    return {
      layout,
      root: root ? nodeMap.get(root.name) || root : null,
      widgets: validWidgets,
      nodeMap,
      messages,
      blockingErrors,
      resolution,
      stats: {
        widgetName: widgetName && widgetName.trim() ? widgetName : "-",
        resolution: `${resolution.width} x ${resolution.height}`,
        rootName: root?.name || "-",
        widgetCount: validWidgets.length,
        warnings: warningCount,
        skipped,
        supported: validWidgets.length + (root?.type === "CanvasPanel" ? 1 : 0),
        unsupported
      }
    };
  }
}

function normalizeResolution(resolution) {
  const width = Number(resolution?.width);
  const height = Number(resolution?.height);
  return {
    width: Number.isFinite(width) && width > 0 ? width : DEFAULT_RESOLUTION.width,
    height: Number.isFinite(height) && height > 0 ? height : DEFAULT_RESOLUTION.height
  };
}

function normalizeWidget(rawWidget, index) {
  const widget = rawWidget && typeof rawWidget === "object" ? { ...rawWidget } : {};
  widget.type = stringOr(widget.type, "");
  widget.name = stringOr(widget.name, "");
  widget.parent = stringOr(widget.parent, "");
  widget.text = stringOr(widget.text, "");
  widget.index = index;
  widget.position = normalizeVector(widget.position, { x: 0, y: 0 });
  widget.size = normalizeSize(widget.size, DEFAULT_SIZE);
  widget.hasPosition = hasObject(rawWidget?.position);
  widget.hasSize = hasObject(rawWidget?.size);
  return widget;
}

function validateWidgetFields(widget, index, parent, messages) {
  const colorFields = ["color", "backgroundColor", "normalColor", "hoveredColor", "pressedColor", "disabledColor", "textColor", "tintColor"];
  colorFields.forEach((fieldName) => {
    const value = widget[fieldName];
    if (typeof value === "string" && value.length > 0 && !isValidHexColor(value)) {
      messages.push({
        level: "warning",
        message: `${widget.name}: invalid ${fieldName} '${value}' - using default color.`
      });
    }
  });

  if (widget.justify && !VALID_JUSTIFY.has(widget.justify)) {
    messages.push({
      level: "warning",
      message: `${widget.name}: invalid justify '${widget.justify}' - using left.`
    });
  }

  if (widget.verticalAlign && !VALID_VERTICAL_ALIGN.has(widget.verticalAlign)) {
    messages.push({
      level: "warning",
      message: `${widget.name}: invalid verticalAlign '${widget.verticalAlign}' - using top.`
    });
  }

  if (widget.drawAs && !VALID_DRAW_AS.has(widget.drawAs)) {
    messages.push({
      level: "warning",
      message: `${widget.name}: invalid drawAs '${widget.drawAs}' - using image.`
    });
  }

  if (parent.type === "CanvasPanel") {
    if (!widget.hasPosition) {
      messages.push({
        level: "warning",
        message: `${widget.name}: CanvasPanel child missing position - using {x:0,y:0}.`
      });
    }
    if (!widget.hasSize) {
      messages.push({
        level: "warning",
        message: `${widget.name}: CanvasPanel child missing size - using {width:100,height:30}.`
      });
    }
  }
}

function messagesForWidget(messages, widgetName) {
  return messages.filter((message) => message.message.startsWith(`${widgetName}:`));
}

function renderPreview(result) {
  currentResolution = result.resolution;
  elements.previewCanvas.replaceChildren();
  elements.previewCanvas.style.width = `${result.resolution.width}px`;
  elements.previewCanvas.style.height = `${result.resolution.height}px`;
  elements.widgetNameLabel.textContent = result.stats.widgetName;
  elements.resolutionLabel.textContent = `Resolution: ${result.stats.resolution}`;

  (result.root.children || []).forEach((child) => {
    elements.previewCanvas.appendChild(renderWidget(child, result.root));
  });

  applyZoom();
}

function renderWidget(widget, parent) {
  const element = document.createElement("div");
  element.className = `umg-widget ${classForType(widget.type)}`;
  element.dataset.name = widget.name;
  element.dataset.type = widget.type;

  applyLayout(element, widget, parent);
  applyCommonStyle(element, widget);

  switch (widget.type) {
    case "TextBlock":
      renderTextBlock(element, widget);
      break;
    case "Button":
      renderButton(element, widget);
      break;
    case "Image":
      renderImage(element, widget);
      break;
    case "Border":
      renderBorder(element, widget);
      break;
    case "ProgressBar":
      renderProgressBar(element, widget);
      break;
    case "VerticalBox":
    case "HorizontalBox":
    case "ScrollBox":
    case "Overlay":
    case "SizeBox":
      renderContainer(element, widget);
      break;
    case "Spacer":
      element.title = widget.name;
      break;
    default:
      element.textContent = `${widget.type}: ${widget.name}`;
      break;
  }

  if (["Button", "Border", "VerticalBox", "HorizontalBox", "ScrollBox", "Overlay", "SizeBox"].includes(widget.type)) {
    appendChildren(element, widget);
  }

  return element;
}

function applyLayout(element, widget, parent) {
  const size = widget.size || DEFAULT_SIZE;
  element.style.width = `${size.width}px`;
  element.style.height = `${size.height}px`;

  if (parent.type === "CanvasPanel") {
    element.classList.add("umg-absolute");
    element.style.left = `${widget.position.x}px`;
    element.style.top = `${widget.position.y}px`;
    element.style.zIndex = Number.isFinite(Number(widget.zOrder)) ? Number(widget.zOrder) : 0;
  }
}

function applyCommonStyle(element, widget) {
  const padding = normalizePadding(widget.padding);
  if (padding) {
    element.style.padding = padding;
  }
}

function renderTextBlock(element, widget) {
  element.textContent = widget.text || widget.name;
  element.style.fontSize = `${Number(widget.fontSize) || 14}px`;
  element.style.color = safeColor(widget.color, "#e7faff");
  element.style.textAlign = VALID_JUSTIFY.has(widget.justify) ? widget.justify : "left";
  element.style.justifyContent = justifyToFlex(widget.justify);
  element.style.alignItems = verticalAlignToFlex(widget.verticalAlign);
}

function renderButton(element, widget) {
  element.textContent = widget.text || widget.name;
  element.style.fontSize = `${Number(widget.fontSize) || 14}px`;
  element.style.background = safeColor(widget.normalColor || widget.color, "rgba(32, 217, 255, 0.14)");
  element.style.color = safeColor(widget.textColor, "#e7faff");
  element.style.justifyContent = justifyToFlex(widget.justify || "center");
}

function renderImage(element, widget) {
  const texturePath = widget.texturePath || widget.imagePath || "";
  const drawAs = VALID_DRAW_AS.has(widget.drawAs) ? widget.drawAs : "image";
  element.style.background = safeColor(widget.tintColor || widget.color, "rgba(255, 255, 255, 0.08)");
  element.innerHTML = "";
  const label = document.createElement("span");
  label.textContent = texturePath ? `${drawAs}: ${texturePath}` : `${drawAs}: Image placeholder`;
  element.appendChild(label);
}

function renderBorder(element, widget) {
  element.style.background = safeColor(widget.backgroundColor || widget.color, "rgba(32, 217, 255, 0.08)");
  if ((widget.drawAs || "").toLowerCase() === "roundedbox") {
    element.style.borderRadius = "10px";
  }
}

function renderProgressBar(element, widget) {
  const percent = clamp(Number(widget.percent ?? 0.5), 0, 1);
  const fill = document.createElement("div");
  fill.className = "umg-progress__fill";
  fill.style.width = `${percent * 100}%`;
  fill.style.background = safeColor(widget.color, "#20d9ff");
  element.appendChild(fill);
}

function renderContainer(element, widget) {
  if (!widget.hasSize && ["VerticalBox", "HorizontalBox", "Overlay", "SizeBox"].includes(widget.type)) {
    element.style.width = "";
    element.style.height = "";
  }
}

function appendChildren(element, widget) {
  (widget.children || []).forEach((child) => {
    element.appendChild(renderWidget(child, widget));
  });
}

function renderTree(result) {
  if (!result.root || result.blockingErrors.length > 0) {
    elements.treePanel.textContent = "No valid hierarchy.";
    elements.treePanel.classList.add("empty-panel");
    return;
  }

  elements.treePanel.classList.remove("empty-panel");
  elements.treePanel.replaceChildren(renderTreeNode(result.root, 0));
}

function renderTreeNode(widget, depth) {
  const fragment = document.createDocumentFragment();
  const row = document.createElement("div");
  const hasWarning = (widget.warnings || []).length > 0;
  row.className = `tree-node${hasWarning ? " tree-node--warning" : ""}`;
  row.style.paddingLeft = `${depth * 14}px`;
  row.innerHTML = `${escapeHtml(widget.name)} <span class="tree-node__meta">(${escapeHtml(widget.type)}${widget.parent ? `, parent: ${escapeHtml(widget.parent)}` : ""})</span>${hasWarning ? " !" : ""}`;
  fragment.appendChild(row);
  (widget.children || []).forEach((child) => {
    fragment.appendChild(renderTreeNode(child, depth + 1));
  });
  return fragment;
}

function showValidation(messages) {
  elements.validationPanel.replaceChildren();
  elements.validationPanel.classList.remove("empty-panel");
  messages.forEach((item) => {
    const row = document.createElement("div");
    row.className = `validation-item validation-item--${item.level}`;
    row.textContent = item.message;
    elements.validationPanel.appendChild(row);
  });
}

function setStats(stats) {
  const rows = [
    ["Widget", stats.widgetName],
    ["Resolution", stats.resolution],
    ["Root", stats.rootName],
    ["Widgets", stats.widgetCount],
    ["Warnings", stats.warnings],
    ["Skipped", stats.skipped],
    ["Supported", stats.supported],
    ["Unsupported", stats.unsupported]
  ];
  elements.statsList.replaceChildren();
  rows.forEach(([label, value]) => {
    const dt = document.createElement("dt");
    const dd = document.createElement("dd");
    dt.textContent = label;
    dd.textContent = value;
    elements.statsList.append(dt, dd);
  });
}

function resetView() {
  currentResolution = { ...DEFAULT_RESOLUTION };
  currentZoom = "fit";
  updateZoomButtons();
  clearPreviewOnly();
  elements.widgetNameLabel.textContent = "No widget loaded";
  elements.resolutionLabel.textContent = "Resolution: -";
  setStats({
    widgetName: "-",
    resolution: "-",
    rootName: "-",
    widgetCount: 0,
    warnings: 0,
    skipped: 0,
    supported: 0,
    unsupported: 0
  });
  elements.treePanel.textContent = "No hierarchy yet.";
  elements.treePanel.classList.add("empty-panel");
  elements.validationPanel.textContent = "No validation results yet.";
  elements.validationPanel.classList.add("empty-panel");
}

function clearPreviewOnly(message = "Load a UMGJsonGenerator JSON file to preview layout.") {
  elements.previewCanvas.replaceChildren();
  elements.previewCanvas.style.width = `${currentResolution.width}px`;
  elements.previewCanvas.style.height = `${currentResolution.height}px`;
  const empty = document.createElement("div");
  empty.className = "empty-preview";
  empty.textContent = message;
  elements.previewCanvas.appendChild(empty);
  applyZoom();
}

function applyZoom() {
  const scale = currentZoom === "fit" ? calculateFitScale() : Number(currentZoom);
  elements.previewScaler.style.transform = `scale(${scale})`;
  elements.previewScaler.style.width = `${currentResolution.width * scale}px`;
  elements.previewScaler.style.height = `${currentResolution.height * scale}px`;
}

function calculateFitScale() {
  const availableWidth = Math.max(elements.previewViewport.clientWidth - 36, 100);
  const availableHeight = Math.max(elements.previewViewport.clientHeight - 36, 100);
  return Math.min(availableWidth / currentResolution.width, availableHeight / currentResolution.height, 1);
}

function updateZoomButtons() {
  elements.zoomButtons.forEach((button) => {
    button.classList.toggle("is-active", button.dataset.zoom === String(currentZoom));
  });
}

function classForType(type) {
  return {
    TextBlock: "umg-text",
    Button: "umg-button",
    Image: "umg-image",
    Border: "umg-border",
    VerticalBox: "umg-vbox",
    HorizontalBox: "umg-hbox",
    ProgressBar: "umg-progress",
    ScrollBox: "umg-scroll",
    Overlay: "umg-overlay",
    SizeBox: "umg-sizebox",
    Spacer: "umg-spacer"
  }[type] || "";
}

function safeColor(value, fallback) {
  return isValidHexColor(value) ? value : fallback;
}

function isValidHexColor(value) {
  if (typeof value !== "string" || value.length === 0) {
    return false;
  }
  const clean = value.startsWith("#") ? value.slice(1) : value;
  return /^([0-9a-fA-F]{6}|[0-9a-fA-F]{8})$/.test(clean);
}

function normalizeVector(value, fallback) {
  return {
    x: Number.isFinite(Number(value?.x)) ? Number(value.x) : fallback.x,
    y: Number.isFinite(Number(value?.y)) ? Number(value.y) : fallback.y
  };
}

function normalizeSize(value, fallback) {
  return {
    width: Number.isFinite(Number(value?.width)) ? Number(value.width) : fallback.width,
    height: Number.isFinite(Number(value?.height)) ? Number(value.height) : fallback.height
  };
}

function normalizePadding(value) {
  if (typeof value === "number") {
    return `${value}px`;
  }
  if (!value || typeof value !== "object") {
    return "";
  }
  const top = Number(value.top || 0);
  const right = Number(value.right || 0);
  const bottom = Number(value.bottom || 0);
  const left = Number(value.left || 0);
  return `${top}px ${right}px ${bottom}px ${left}px`;
}

function hasObject(value) {
  return Boolean(value && typeof value === "object" && !Array.isArray(value));
}

function stringOr(value, fallback) {
  return typeof value === "string" ? value : fallback;
}

function justifyToFlex(value) {
  return {
    left: "flex-start",
    center: "center",
    right: "flex-end"
  }[value] || "flex-start";
}

function verticalAlignToFlex(value) {
  return {
    top: "flex-start",
    center: "center",
    bottom: "flex-end"
  }[value] || "flex-start";
}

function clamp(value, min, max) {
  if (!Number.isFinite(value)) {
    return min;
  }
  return Math.max(min, Math.min(max, value));
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;")
    .replaceAll("'", "&#039;");
}
