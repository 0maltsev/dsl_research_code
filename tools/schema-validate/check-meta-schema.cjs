#!/usr/bin/env node
'use strict';

// Dev-only Draft 2020-12 meta-schema conformance check for this
// repository's schemas/*.schema.json.
//
// This is deliberately NOT part of the production C++ pipeline: it exists
// because the only maintained C++ JSON Schema library available here
// (pboettch/json-schema-validator, used in src/support/record_schema.cpp for
// everyday record validation against our own schemas) has no
// $dynamicRef/$dynamicAnchor support and therefore cannot correctly process
// the official, self-referential Draft 2020-12 meta-schema. ajv's Ajv2020
// class does implement $dynamicRef/$dynamicAnchor and ships the real
// 2020-12 meta-schema, so it is used here as a one-time, non-Python,
// non-production verification tool -- the same role wasm-validate (WABT)
// plays later for Wasm validation. See docs/dependency-policy.md.
//
// `strict: false` disables Ajv's own opinionated linting (e.g. it would
// otherwise flag a valid "type" array such as
// correctness-result.schema.json's `"seed": {"type": ["string","integer","null"]}`
// as a style concern); it does not relax Draft 2020-12 conformance.
// `validateSchema` is left at its default `true`: that default is the
// actual meta-schema check, since Ajv validates each schema document
// against its built-in official 2020-12 meta-schema before compiling it,
// and ajv.compile() throws if that validation fails.

const fs = require('fs');
const path = require('path');
const Ajv2020 = require('ajv/dist/2020').default;
const addFormats = require('ajv-formats');

const schemasDir = path.resolve(__dirname, '..', '..', 'schemas');
const schemaFiles = fs
  .readdirSync(schemasDir)
  .filter((name) => name.endsWith('.schema.json'))
  .sort();

if (schemaFiles.length === 0) {
  console.error(`no *.schema.json files found under ${schemasDir}`);
  process.exit(1);
}

let failed = false;

for (const fileName of schemaFiles) {
  const filePath = path.join(schemasDir, fileName);
  const ajv = new Ajv2020({ strict: false, validateSchema: true, allErrors: true });
  addFormats(ajv);

  const schema = JSON.parse(fs.readFileSync(filePath, 'utf8'));
  try {
    ajv.compile(schema);
    console.log(`PASS ${fileName}: valid Draft 2020-12 schema`);
  } catch (error) {
    failed = true;
    console.error(`FAIL ${fileName}: ${error.message}`);
  }
}

process.exit(failed ? 1 : 0);
