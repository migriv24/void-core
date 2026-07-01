'use strict';
// Logging spine (Codex §6). One logger shared by GUI, CLI, and scripts.
// - timestamped, levelled lines
// - in-memory ring buffer (for "Copy log")
// - persisted to logs/void.log
// - emits events so the server can stream over SSE
// - run() streams a child process line-by-line through the same pipe

const fs = require('fs');
const path = require('path');
const { EventEmitter } = require('events');
const { spawn } = require('child_process');

const LEVELS = { INFO: 1, WARN: 2, ERROR: 3 };

function createLogger({ file, bufferSize = 2000 } = {}) {
  const emitter = new EventEmitter();
  emitter.setMaxListeners(0);
  const buffer = [];
  let stream = null;

  if (file) {
    try {
      fs.mkdirSync(path.dirname(file), { recursive: true });
      stream = fs.createWriteStream(file, { flags: 'a' });
    } catch { stream = null; }
  }

  function emit(level, op, message) {
    const ts = new Date().toISOString();
    const line = `[${ts}] ${level} ${op}: ${message}`;
    buffer.push({ ts, level, op, message, line });
    if (buffer.length > bufferSize) buffer.shift();
    if (stream) stream.write(line + '\n');
    emitter.emit('line', { ts, level, op, message, line });
    return line;
  }

  const log = {
    info: (op, m) => emit('INFO', op, m),
    warn: (op, m) => emit('WARN', op, m),
    error: (op, m) => emit('ERROR', op, m),
    raw: (op, m) => emit('INFO', op, m),

    on: (ev, fn) => emitter.on(ev, fn),
    off: (ev, fn) => emitter.off(ev, fn),

    tail(n = 100, level) {
      let lines = buffer;
      if (level) lines = lines.filter(l => LEVELS[l.level] >= (LEVELS[level] || 1));
      return lines.slice(-n);
    },
    text(n) { return this.tail(n || buffer.length).map(l => l.line).join('\n'); },

    // Stream a child process line-by-line through the logger.
    // Returns a promise resolving to the exit code. `op` labels the lines.
    run(cmd, args = [], opts = {}, op = 'run') {
      return new Promise((resolve) => {
        emit('INFO', op, `$ ${cmd} ${args.join(' ')}`.trim());
        const proc = spawn(cmd, args, { shell: true, ...opts });
        const onData = (level) => (d) =>
          d.toString().split('\n').forEach(s => { if (s.trim()) emit(level, op, s.trim()); });
        proc.stdout.on('data', onData('INFO'));
        proc.stderr.on('data', onData('INFO')); // tools log progress to stderr; keep it
        proc.on('error', e => { emit('ERROR', op, e.message); resolve(1); });
        proc.on('close', code => {
          emit(code === 0 ? 'INFO' : 'ERROR', op, `exited ${code}`);
          resolve(code);
        });
      });
    },
  };

  return log;
}

module.exports = { createLogger, LEVELS };
