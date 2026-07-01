#!/usr/bin/env node
'use strict';
// Void Core CLI + REPL. Drives the same dispatcher the GUI uses.
//   void --state ./state.json describe          # one-shot
//   void --state ./state.json                    # interactive REPL
//   void ... --json                              # machine output
// A twin can ship its own thin CLI that calls createManager() with its glyphs
// and adapters, then hands argv here via runCli().

const path = require('path');
const readline = require('readline');
const { createManager } = require('../index');

function pluck(argv, flag) {
  const i = argv.indexOf(flag);
  if (i === -1) return null;
  const val = argv[i + 1];
  argv.splice(i, 2);
  return val;
}
function pluckBool(argv, flag) {
  const i = argv.indexOf(flag);
  if (i === -1) return false;
  argv.splice(i, 1);
  return true;
}

function printResult(r, asJson) {
  if (asJson) { console.log(JSON.stringify(r.data ?? r.lines, null, 2)); return; }
  for (const line of r.lines || []) console.log(line);
}

async function runCli(argv, managerOpts = {}) {
  const stateFile = pluck(argv, '--state') || managerOpts.stateFile || path.resolve(process.cwd(), 'state.json');
  const asJson = pluckBool(argv, '--json-out');
  const manager = createManager({ ...managerOpts, stateFile });
  const { dispatch, store, state } = manager;

  // mirror streamed log lines to stderr so long ops (deploy/preview) show live
  manager.logger.on('line', l => { if (l.op === 'deploy' || l.op === 'build' || l.op === 'preview') process.stderr.write(l.line + '\n'); });

  if (argv.length) {
    const r = await dispatch(argv);
    printResult(r, asJson || argv.includes('--json'));
    store.save(state);                       // persist working state (not Save Progress)
    process.exit(r.ok ? 0 : 1);
  }

  // ── REPL ──
  console.log(`void-core REPL  ·  state: ${stateFile}  ·  type \`help\` or \`exit\``);
  const rl = readline.createInterface({ input: process.stdin, output: process.stdout, prompt: 'void> ' });
  rl.prompt();
  rl.on('line', async (line) => {
    const cmd = line.trim();
    if (!cmd) { rl.prompt(); return; }
    const r = await dispatch(cmd);
    printResult(r, cmd.includes('--json'));
    store.save(state);
    if (r.data && r.data.signal === 'exit') { rl.close(); return; }
    rl.prompt();
  });
  rl.on('close', () => { store.save(state); process.exit(0); });
}

if (require.main === module) {
  runCli(process.argv.slice(2)).catch(e => { console.error(e); process.exit(1); });
}

module.exports = { runCli };
