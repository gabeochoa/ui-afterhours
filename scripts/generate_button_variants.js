#!/usr/bin/env node
const fs = require('fs');
const path = require('path');

const REPO = path.resolve(__dirname, '..');
const ACTIONS_DIR = path.join(REPO, 'actions');
const TEMPLATE_JSON = path.join(ACTIONS_DIR, 'single_button', 'single_button.json');

const colors = ['Primary','Secondary','Accent','Error','Background'];
const bools = [true,false];

function ensureDir(p) {
  fs.mkdirSync(p, { recursive: true });
}

function writeFile(p, c) {
  fs.writeFileSync(p, c, 'utf8');
}

function main() {
  const template = fs.readFileSync(TEMPLATE_JSON, 'utf8');

  for (const color of colors) {
    for (const hasLabel of bools) {
      for (const disabled of bools) {
        const slug = `button_${color.toLowerCase()}_label_${hasLabel}_disabled_${disabled}`;
        const dir = path.join(ACTIONS_DIR, slug);
        ensureDir(dir);

        // TOML
        const toml = [
          'autoquit = true',
          'dump_path = "ui_tree.json"',
          '',
          '[button]',
          `hasLabel = ${hasLabel}`,
          `color = "${color}"`,
          `disabled = ${disabled}`,
          '',
          '[[step]]',
          'pressed = ["WidgetNext"]',
          '',
          '[[step]]',
          'pressed = ["WidgetPress"]',
          ''
        ].join('\n');
        writeFile(path.join(dir, `${slug}.toml`), toml);

        // meta.json
        const meta = {
          type: 'button',
          hasLabel,
          color,
          disabled
        };
        writeFile(path.join(dir, 'meta.json'), JSON.stringify(meta, null, 2));

        // expected JSON - reuse template
        writeFile(path.join(dir, `${slug}.json`), template);
      }
    }
  }
  console.log('Generated button variant scenarios.');
}

if (require.main === module) {
  main();
}