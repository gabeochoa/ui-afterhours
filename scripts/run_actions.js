#!/usr/bin/env node
/*
 Run all action scenarios under actions/ and validate ui_tree.json

 - Each scenario lives under actions/<scenario>/
   - Must include exactly one .toml file (input playback)
   - Must include exactly one .json file (expected UI tree subset)

 Validation rules:
 - Expected JSON is a subset matcher by name: every expected node must exist
   in the actual tree at the corresponding position in the tree (by name),
   and if expected provides a rect, its fields are compared with a tolerance.
 - Extra actual nodes are ignored.
*/

const fs = require('fs');
const path = require('path');
const { spawnSync } = require('child_process');

const REPO_ROOT = path.resolve(__dirname, '..');
const ACTIONS_DIR = path.join(REPO_ROOT, 'actions');
const UI_EXE = path.join(REPO_ROOT, 'ui.exe');
const ACTUAL_JSON = path.join(REPO_ROOT, 'ui_tree.json');

const TOLERANCE = parseFloat(process.env.UI_POS_TOL || '0.5');

function readJson(file) {
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function approxEqual(a, b, tol = TOLERANCE) {
  return Math.abs((a ?? 0) - (b ?? 0)) <= tol;
}

function matchNode(expected, actual, errs, pathStr) {
  // Name must match if provided
  if (expected.name !== undefined) {
    if (actual.name !== expected.name) {
      errs.push(`name mismatch at ${pathStr}: expected '${expected.name}', got '${actual.name}'`);
      return false;
    }
  }
  // Rect compare if present
  if (expected.rect) {
    const er = expected.rect, ar = actual.rect || {};
    if (!approxEqual(er.x, ar.x) || !approxEqual(er.y, ar.y) || !approxEqual(er.w, ar.w) || !approxEqual(er.h, ar.h)) {
      errs.push(`rect mismatch at ${pathStr}: expected ${JSON.stringify(er)}, got ${JSON.stringify(ar)}`);
      return false;
    }
  }
  // Children subset match by name in order
  const expChildren = expected.children || [];
  const actChildren = actual.children || [];
  let ai = 0;
  for (let i = 0; i < expChildren.length; i++) {
    const expChild = expChildren[i];
    // Find next actual child with matching name
    let matchIdx = -1;
    for (; ai < actChildren.length; ai++) {
      if (expChild.name === undefined || actChildren[ai].name === expChild.name) {
        matchIdx = ai;
        ai++; // next search starts after this
        break;
      }
    }
    if (matchIdx === -1) {
      errs.push(`missing child at ${pathStr}: '${expChild.name ?? '(unnamed)'}'`);
      return false;
    }
    const childOk = matchNode(expChild, actChildren[matchIdx], errs, `${pathStr}/${expChild.name ?? matchIdx}`);
    if (!childOk) return false;
  }
  return true;
}

function runScenario(dir) {
  const files = fs.readdirSync(dir);
  const tomls = files.filter(f => f.toLowerCase().endsWith('.toml'));
  // Ignore meta.json when selecting the expected JSON file
  const jsons = files.filter(f => f.toLowerCase().endsWith('.json') && f.toLowerCase() !== 'meta.json');
  if (tomls.length !== 1 || jsons.length !== 1) {
    throw new Error(`Scenario '${path.basename(dir)}' must contain exactly one .toml and one .json`);
  }
  const tomlPath = path.join(dir, tomls[0]);
  const expectedPath = path.join(dir, jsons[0]);

  // Optional metadata tags per scenario
  let meta = {};
  const metaPath = path.join(dir, 'meta.json');
  if (fs.existsSync(metaPath)) {
    try {
      meta = readJson(metaPath);
    } catch (e) {
      console.warn(`[WARN] Invalid meta.json in '${path.basename(dir)}': ${e.message}`);
    }
  }

  // Run ui.exe with actions
  const run = spawnSync(UI_EXE, [ `--actions=${tomlPath}` ], { cwd: REPO_ROOT, stdio: 'inherit' });
  if (run.status !== 0) {
    throw new Error(`ui.exe exited with code ${run.status} for scenario '${path.basename(dir)}'`);
  }
  if (!fs.existsSync(ACTUAL_JSON)) {
    throw new Error(`ui_tree.json not produced for scenario '${path.basename(dir)}'`);
  }

  const expected = readJson(expectedPath);
  const actual = readJson(ACTUAL_JSON);
  const errs = [];
  const ok = matchNode(expected.root, actual.root, errs, 'root');
  return { ok, errs, meta };
}

function findScenarios(rootDir, filter) {
  const entries = fs.readdirSync(rootDir, { withFileTypes: true });
  return entries
    .filter(d => d.isDirectory())
    .map(d => path.join(rootDir, d.name))
    .filter(dir => !filter || dir.includes(filter));
}

function main() {
  const filter = process.argv[2] || '';
  if (!fs.existsSync(UI_EXE)) {
    console.error(`Missing binary at ${UI_EXE}. Build first (make).`);
    process.exit(2);
  }
  const scenarios = findScenarios(ACTIONS_DIR, filter);
  if (scenarios.length === 0) {
    console.error('No scenarios found under actions/');
    process.exit(2);
  }

  let passed = 0;
  let failed = 0;
  const results = [];
  const tags = [];
  for (const dir of scenarios) {
    const name = path.basename(dir);
    try {
      const { ok, errs, meta } = runScenario(dir);
      if (ok) {
        console.log(`[PASS] ${name}`);
        passed++;
        results.push({ name, ok });
        tags.push({ name, meta });
      } else {
        console.log(`[FAIL] ${name}`);
        errs.forEach(e => console.log('  - ' + e));
        failed++;
        results.push({ name, ok, errs });
      }
    } catch (e) {
      console.log(`[ERROR] ${name}: ${e.message}`);
      failed++;
      results.push({ name, ok: false, error: e.message });
    }
  }

  console.log(`\nSummary: ${passed} passed, ${failed} failed`);

   // Coverage reporting for button variants
   const REQUIRED = {
     button: {
       hasLabel: [true, false],
       color: ['Primary','Secondary','Accent','Error','Background'],
       disabled: [true, false],
     }
   };

   const buttonTags = tags.filter(t => t.meta && t.meta.type === 'button');
   const seen = new Set();
   for (const t of buttonTags) {
     const hl = String(t.meta.hasLabel);
     const color = String(t.meta.color);
     const dis = String(t.meta.disabled);
     seen.add(`${hl}|${color}|${dis}`);
   }

   const missing = [];
   for (const hl of REQUIRED.button.hasLabel) {
     for (const color of REQUIRED.button.color) {
       for (const dis of REQUIRED.button.disabled) {
         const key = `${hl}|${color}|${dis}`;
         if (!seen.has(key)) missing.push({ hasLabel: hl, color, disabled: dis });
       }
     }
   }

   if (missing.length > 0) {
     console.log(`\n[Coverage] Missing button combinations (${missing.length}):`);
     for (const m of missing) {
       console.log(`  - hasLabel=${m.hasLabel}, color=${m.color}, disabled=${m.disabled}`);
     }
     if (process.env.REQUIRE_COVERAGE === '1') {
       process.exit(1);
     }
   } else {
     console.log(`\n[Coverage] Button variants fully covered.`);
   }

  process.exit(failed === 0 ? 0 : 1);
}

if (require.main === module) {
  main();
}

