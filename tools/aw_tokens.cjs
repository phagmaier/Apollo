// Prints aw-parser's reading of a Fountain file as JSON. aw-parser (MIT) is
// afterwriting's independent Fountain parser, installed with the pinned
// afterwriting 1.17.3 by tools/afterwriting-oracle.sh; it is an oracle for
// Apollo's Fountain export, never a dependency of the app.
//
//   node tools/aw_tokens.cjs AW_PARSER_DIR FILE.fountain
"use strict";
const fs = require("fs");
const path = require("path");
const [dir, file] = process.argv.slice(2);
if (!dir || !file) { console.error("usage: aw_tokens.cjs AW_PARSER_DIR FILE.fountain"); process.exit(2); }
const version = require(path.join(dir, "package.json")).version;
if (version !== "0.1.1") { console.error(`expected aw-parser 0.1.1, found ${version}`); process.exit(2); }
const { parser } = require(dir);
const config = {
  print_actions: true, print_headers: true, print_dialogues: true, print_notes: false, print_sections: false,
  print_synopsis: false, use_dual_dialogue: true, merge_multiple_empty_lines: false,
  double_space_between_scenes: false, each_scene_on_new_page: false,
};
const result = parser.parse(fs.readFileSync(file, "utf8"), config);
const plain = (token) => ({ type: token.type, text: token.text, dual: token.dual || null });
process.stdout.write(JSON.stringify({ title_page: result.title_page.map(plain), tokens: result.tokens.map(plain) }, null, 1) + "\n");
