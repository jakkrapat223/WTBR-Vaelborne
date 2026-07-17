// HTML → UMG JSON Converter
//
// Loads a prototype HTML page into a same-origin iframe at the target
// resolution, walks the rendered DOM, and maps elements to the
// UMGJsonGenerator JSON schema (flat children of RootCanvas, absolute
// position/size, matching the convention used by Examples/GameUI/*.json).
//
// Mapping rules (in priority order per element):
//   1. data-umg="skip"            → skip element + subtree
//   2. data-umg="ignore"          → emit nothing for this element, still descend
//   3. data-umg-type="<Type>"     → force widget type (consumes subtree for
//                                    leaf types: TextBlock/Image/ProgressBar/Button)
//   4. <img>                      → Image
//   5. <button> / role="button"   → Button (label = textContent)
//   6. class contains bar/progress/meter + visible bg → ProgressBar
//      (percent + fill color measured from the first child "fill" element)
//   7. visible background/border  → Border (still descends into children)
//   8. leaf with text             → TextBlock
//   9. mixed text + elements      → TextBlock per direct text node (Range-measured)
//
// Naming: data-umg-name > #id > first CSS class, PascalCased, with the
// project prefix per type (Txt_/Border_/Img_/PB_/Btn_). Names are de-duped
// with numeric suffixes.

(() => {
  "use strict";

  const els = {
    srcUrl: document.getElementById("srcUrl"),
    rootSelector: document.getElementById("rootSelector"),
    resW: document.getElementById("resW"),
    resH: document.getElementById("resH"),
    widgetName: document.getElementById("widgetName"),
    parentClass: document.getElementById("parentClass"),
    skipAriaHidden: document.getElementById("skipAriaHidden"),
    roundValues: document.getElementById("roundValues"),
    loadBtn: document.getElementById("loadBtn"),
    convertBtn: document.getElementById("convertBtn"),
    downloadBtn: document.getElementById("downloadBtn"),
    copyBtn: document.getElementById("copyBtn"),
    frame: document.getElementById("frame"),
    stageInfo: document.getElementById("stageInfo"),
    stageOuter: document.getElementById("stageOuter"),
    stageScaler: document.getElementById("stageScaler"),
    outJson: document.getElementById("outJson"),
    statsBox: document.getElementById("statsBox"),
    logBox: document.getElementById("logBox")
  };

  const TYPE_PREFIX = {
    TextBlock: "Txt_",
    Border: "Border_",
    Image: "Img_",
    ProgressBar: "PB_",
    Button: "Btn_"
  };

  const LEAF_TYPES = new Set(["TextBlock", "Image", "ProgressBar", "Button", "Spacer"]);

  let captureRoot = null;
  let frameWin = null;

  // ── Logging ────────────────────────────────────────────────────────────────

  function clearLog() {
    els.logBox.textContent = "";
  }

  function log(level, message) {
    const line = document.createElement("div");
    line.className = "log-" + level;
    line.textContent = (level === "err" ? "✖ " : level === "warn" ? "⚠ " : "· ") + message;
    els.logBox.appendChild(line);
  }

  // ── Page loading ───────────────────────────────────────────────────────────

  els.loadBtn.addEventListener("click", () => {
    clearLog();
    els.convertBtn.disabled = true;
    captureRoot = null;

    const w = parseInt(els.resW.value, 10) || 1920;
    const h = parseInt(els.resH.value, 10) || 1080;
    els.frame.width = w;
    els.frame.height = h;

    const url = els.srcUrl.value.trim();
    if (!url) {
      log("err", "No URL given.");
      return;
    }

    els.stageInfo.textContent = "Loading " + url + " …";
    const bust = (url.includes("?") ? "&" : "?") + "umgc=" + Date.now();
    els.frame.src = url + bust;
  });

  els.frame.addEventListener("load", () => {
    if (!els.frame.src || els.frame.src === "about:blank") {
      return;
    }
    let doc = null;
    try {
      doc = els.frame.contentDocument;
      if (!doc || !doc.body) throw new Error("no document");
    } catch (e) {
      log("err", "Cannot access the loaded page (cross-origin?). Serve the prototype from the same static server as this converter.");
      els.stageInfo.textContent = "Load failed.";
      return;
    }

    // Give web fonts / late layout a moment to settle before measuring.
    setTimeout(() => {
      frameWin = els.frame.contentWindow;
      const sel = els.rootSelector.value.trim() || "body";
      captureRoot = doc.querySelector(sel);
      if (!captureRoot) {
        log("warn", 'Selector "' + sel + '" not found — falling back to <body>.');
        captureRoot = doc.body;
      }
      const rr = captureRoot.getBoundingClientRect();
      els.stageInfo.textContent =
        "Loaded. Capture root: " + describeElement(captureRoot) +
        " — rendered " + Math.round(rr.width) + "x" + Math.round(rr.height) + " px.";
      els.convertBtn.disabled = false;
      fitStage();
    }, 350);
  });

  function fitStage() {
    const w = parseInt(els.resW.value, 10) || 1920;
    const outerW = els.stageOuter.clientWidth || 1;
    const scale = Math.min(1, outerW / w);
    els.stageScaler.style.transform = "scale(" + scale + ")";
  }
  window.addEventListener("resize", fitStage);

  function describeElement(el) {
    let s = "<" + el.tagName.toLowerCase() + ">";
    if (el.id) s += "#" + el.id;
    else if (el.classList.length) s += "." + el.classList[0];
    return s;
  }

  // ── Color helpers ──────────────────────────────────────────────────────────

  function cssColorToHex(cssColor, extraAlpha) {
    if (!cssColor) return null;
    const m = cssColor.match(/rgba?\(([^)]+)\)/);
    if (!m) return null;
    const parts = m[1].split(",").map((p) => parseFloat(p.trim()));
    if (parts.length < 3 || parts.some((p) => Number.isNaN(p))) return null;
    const a = (parts.length > 3 ? parts[3] : 1) * (extraAlpha == null ? 1 : extraAlpha);
    if (a <= 0.004) return null; // fully transparent
    const to2 = (v) => Math.max(0, Math.min(255, Math.round(v))).toString(16).padStart(2, "0").toUpperCase();
    const rgb = "#" + to2(parts[0]) + to2(parts[1]) + to2(parts[2]);
    return a >= 0.996 ? rgb : rgb + to2(a * 255);
  }

  function effectiveOpacity(el, stopAt) {
    let o = 1;
    let node = el;
    while (node && node !== stopAt.parentElement && node.nodeType === 1) {
      const v = parseFloat(frameWin.getComputedStyle(node).opacity);
      if (!Number.isNaN(v)) o *= v;
      node = node.parentElement;
    }
    return o;
  }

  // ── Naming ─────────────────────────────────────────────────────────────────

  function pascalCase(str) {
    return (str || "")
      .split(/[^a-zA-Z0-9]+/)
      .filter(Boolean)
      .map((p) => p.charAt(0).toUpperCase() + p.slice(1))
      .join("");
  }

  function makeUniqueName(desired, usedNames) {
    let name = desired;
    let i = 2;
    while (usedNames.has(name)) {
      name = desired + "_" + i;
      i += 1;
    }
    usedNames.add(name);
    return name;
  }

  function nameFor(type, el, usedNames, fallbackHint) {
    const explicit = el && el.getAttribute && el.getAttribute("data-umg-name");
    if (explicit) {
      return makeUniqueName(explicit, usedNames);
    }
    const prefix = TYPE_PREFIX[type] || (type + "_");
    let base = "";
    if (el && el.id) base = pascalCase(el.id);
    else if (el && el.classList && el.classList.length) base = pascalCase(el.classList[0]);
    if (!base) base = pascalCase(fallbackHint) || type;
    return makeUniqueName(prefix + base, usedNames);
  }

  // ── Conversion ─────────────────────────────────────────────────────────────

  els.convertBtn.addEventListener("click", () => {
    clearLog();
    if (!captureRoot || !frameWin) {
      log("err", "Load a page first.");
      return;
    }

    const round = els.roundValues.checked;
    const skipAria = els.skipAriaHidden.checked;
    const resW = parseInt(els.resW.value, 10) || 1920;
    const resH = parseInt(els.resH.value, 10) || 1080;

    const rootRect = captureRoot.getBoundingClientRect();
    // Neutralize any ancestor transform scaling (e.g. a stage that scales to fit).
    const scaleX = captureRoot.offsetWidth ? rootRect.width / captureRoot.offsetWidth : 1;
    const scaleY = captureRoot.offsetHeight ? rootRect.height / captureRoot.offsetHeight : 1;

    const ROOT_NAME = "RootCanvas";
    const usedNames = new Set([ROOT_NAME]);
    const widgets = [];
    const warnings = [];
    let zCounter = 0;
    let skipped = 0;

    const num = (v) => (round ? Math.round(v) : Math.round(v * 100) / 100);

    function rectOf(target) {
      const r = target.getBoundingClientRect();
      return {
        x: num((r.left - rootRect.left) / scaleX),
        y: num((r.top - rootRect.top) / scaleY),
        w: num(r.width / scaleX),
        h: num(r.height / scaleY)
      };
    }

    function push(type, name, rect, props) {
      const widget = Object.assign(
        {
          type: type,
          name: name,
          parent: ROOT_NAME,
          position: { x: rect.x, y: rect.y },
          size: { width: rect.w, height: rect.h },
          zOrder: zCounter++
        },
        props || {}
      );
      widgets.push(widget);
    }

    function directText(el) {
      let t = "";
      for (const n of el.childNodes) {
        if (n.nodeType === 3) t += n.textContent;
      }
      return t.replace(/\s+/g, " ").trim();
    }

    function textProps(el, cs) {
      const props = {
        text: "",
        fontSize: Math.max(1, Math.round(parseFloat(cs.fontSize) || 12))
      };
      const color = cssColorToHex(cs.color, effectiveOpacity(el, captureRoot));
      if (color) props.color = color;
      const ta = cs.textAlign;
      if (ta === "center") props.justify = "center";
      else if (ta === "right" || ta === "end") props.justify = "right";
      return props;
    }

    function emitTextBlock(el, cs, text, rect, hint) {
      const props = textProps(el, cs);
      props.text = text;
      push("TextBlock", nameFor("TextBlock", el, usedNames, hint || text), rect, props);
    }

    function emitProgressBar(el, cs, rect) {
      const props = {};
      // Fill color + percent from the first child that looks like a fill bar.
      const fill = el.children[0];
      if (fill) {
        const fr = fill.getBoundingClientRect();
        const er = el.getBoundingClientRect();
        if (er.width > 0) {
          props.percent = Math.round((fr.width / er.width) * 100) / 100;
        }
        const fillColor = cssColorToHex(
          frameWin.getComputedStyle(fill).backgroundColor,
          effectiveOpacity(fill, captureRoot)
        );
        if (fillColor) props.color = fillColor;
      } else {
        const own = cssColorToHex(cs.backgroundColor, effectiveOpacity(el, captureRoot));
        if (own) props.color = own;
      }
      push("ProgressBar", nameFor("ProgressBar", el, usedNames), rect, props);
    }

    function emitBorder(el, cs, rect) {
      const props = {};
      const opacity = effectiveOpacity(el, captureRoot);
      let color = cssColorToHex(cs.backgroundColor, opacity);
      if (!color) {
        color = cssColorToHex(cs.borderTopColor, opacity * 0.6);
        if (color) {
          warnings.push(
            describeElement(el) + ": border-only box approximated as a filled Border — adjust color/alpha manually."
          );
        }
      }
      if (color) props.color = color;
      if (cs.backgroundImage && cs.backgroundImage !== "none") {
        warnings.push(describeElement(el) + ": background-image/gradient not convertible — flat color used.");
      }
      push("Border", nameFor("Border", el, usedNames), rect, props);
    }

    function visit(el) {
      if (el.nodeType !== 1) return;
      const tag = el.tagName;
      if (tag === "SCRIPT" || tag === "STYLE" || tag === "LINK" || tag === "META") return;

      const umg = el.getAttribute("data-umg");
      if (umg === "skip") return;
      if (skipAria && el.getAttribute("aria-hidden") === "true" && umg !== "keep") return;

      const cs = frameWin.getComputedStyle(el);
      if (cs.display === "none" || cs.visibility === "hidden") return;

      const rect = rectOf(el);
      const visible = rect.w >= 1 && rect.h >= 1;
      const forcedType = el.getAttribute("data-umg-type");
      const ignoreSelf = umg === "ignore";

      if (visible && !ignoreSelf) {
        // 1) Forced type
        if (forcedType) {
          if (forcedType === "TextBlock") {
            emitTextBlock(el, cs, el.textContent.replace(/\s+/g, " ").trim(), rect);
            return;
          }
          if (forcedType === "ProgressBar") { emitProgressBar(el, cs, rect); return; }
          if (forcedType === "Image") {
            const props = {};
            const tint = cssColorToHex(cs.color, effectiveOpacity(el, captureRoot));
            if (tint) props.color = tint;
            push("Image", nameFor("Image", el, usedNames), rect, props);
            warnings.push(describeElement(el) + ": Image emitted without imagePath — assign a texture in the JSON.");
            return;
          }
          if (forcedType === "Button") {
            const props = textProps(el, cs);
            props.text = el.textContent.replace(/\s+/g, " ").trim();
            push("Button", nameFor("Button", el, usedNames), rect, props);
            return;
          }
          if (forcedType === "Border") {
            emitBorder(el, cs, rect);
            // fall through to children
          } else if (forcedType === "Spacer") {
            push("Spacer", nameFor("Spacer", el, usedNames, "Spacer"), rect, {});
            return;
          } else if (!LEAF_TYPES.has(forcedType) && forcedType !== "Border") {
            warnings.push(describeElement(el) + ': unsupported data-umg-type "' + forcedType + '" — classified automatically instead.');
          }
        }

        if (!forcedType) {
          // 2) <img>
          if (tag === "IMG") {
            push("Image", nameFor("Image", el, usedNames), rect, {});
            warnings.push(describeElement(el) + ': <img src="' + (el.getAttribute("src") || "") + '"> — set imagePath to a UE texture path manually.');
            return;
          }

          // 3) <button> / role=button
          if (tag === "BUTTON" || el.getAttribute("role") === "button") {
            const props = textProps(el, cs);
            props.text = el.textContent.replace(/\s+/g, " ").trim();
            push("Button", nameFor("Button", el, usedNames), rect, props);
            return;
          }

          // 4) progress-bar heuristic
          const classTokens = Array.from(el.classList).join(" ").toLowerCase();
          const looksLikeBar = /(^|[-_ ])(bar|progress|meter)($|[-_ ])/.test(classTokens);
          const bg = cssColorToHex(cs.backgroundColor, 1);
          if (looksLikeBar && (bg || el.children.length === 1)) {
            emitProgressBar(el, cs, rect);
            return;
          }

          const hasElementChildren = el.children.length > 0;
          const hasBackground =
            !!bg ||
            (cs.backgroundImage && cs.backgroundImage !== "none") ||
            (parseFloat(cs.borderTopWidth) > 0 && cssColorToHex(cs.borderTopColor, 1));

          // 5) visible box → Border (and keep descending)
          if (hasBackground) {
            emitBorder(el, cs, rect);
          }

          // 6) leaf with text → TextBlock (on top of its Border if one was emitted)
          if (!hasElementChildren) {
            const text = directText(el);
            if (text) {
              emitTextBlock(el, cs, text, rect, text.split(" ")[0]);
            } else if (!hasBackground) {
              skipped += 1;
            }
            return;
          }

          // 7) mixed content: emit a TextBlock per direct text node (Range-measured)
          for (const n of el.childNodes) {
            if (n.nodeType !== 3) continue;
            const t = n.textContent.replace(/\s+/g, " ").trim();
            if (!t) continue;
            const range = el.ownerDocument.createRange();
            range.selectNodeContents(n);
            const r = range.getBoundingClientRect();
            if (r.width < 1 || r.height < 1) continue;
            const trect = {
              x: num((r.left - rootRect.left) / scaleX),
              y: num((r.top - rootRect.top) / scaleY),
              w: num(r.width / scaleX),
              h: num(r.height / scaleY)
            };
            emitTextBlock(el, cs, t, trect, t.split(" ")[0]);
          }
        }
      }

      // Descend
      for (const child of el.children) visit(child);
    }

    for (const child of captureRoot.children) visit(child);

    const doc = {
      widgetName: els.widgetName.value.trim() || "WBP_Converted",
      resolution: { width: resW, height: resH },
      root: { type: "CanvasPanel", name: ROOT_NAME },
      widgets: widgets
    };
    const parentClass = els.parentClass.value.trim();
    if (parentClass) doc.parentClass = parentClass;

    els.outJson.value = JSON.stringify(doc, null, 2);
    els.downloadBtn.disabled = false;
    els.copyBtn.disabled = false;

    // Stats
    const counts = {};
    for (const wgt of widgets) counts[wgt.type] = (counts[wgt.type] || 0) + 1;
    els.statsBox.innerHTML =
      "<span>Widgets: <b>" + widgets.length + "</b></span>" +
      Object.keys(counts).sort().map((t) => "<span>" + t + ": <b>" + counts[t] + "</b></span>").join("") +
      "<span>Skipped empties: <b>" + skipped + "</b></span>" +
      "<span>Warnings: <b>" + warnings.length + "</b></span>";

    log("info", "Converted " + widgets.length + " widgets from " + describeElement(captureRoot) + ".");
    for (const w of warnings) log("warn", w);
    log("info", "Validate in index.html (paste JSON) before importing into Unreal.");
  });

  // ── Output actions ─────────────────────────────────────────────────────────

  els.downloadBtn.addEventListener("click", () => {
    const name = (els.widgetName.value.trim() || "WBP_Converted") + ".json";
    const blob = new Blob([els.outJson.value], { type: "application/json" });
    const a = document.createElement("a");
    a.href = URL.createObjectURL(blob);
    a.download = name;
    a.click();
    URL.revokeObjectURL(a.href);
  });

  els.copyBtn.addEventListener("click", async () => {
    try {
      await navigator.clipboard.writeText(els.outJson.value);
      log("info", "JSON copied to clipboard.");
    } catch (e) {
      els.outJson.select();
      document.execCommand("copy");
      log("info", "JSON copied (fallback).");
    }
  });
})();
