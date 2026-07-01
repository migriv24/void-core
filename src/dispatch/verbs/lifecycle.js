'use strict';
// verbs/lifecycle.js — the "lifecycle" verb family (state lifecycle + host effects) for the dispatcher.
// A factory over shared deps; mirrors the C core's verbs_lifecycle.c split. Handler
// bodies are unchanged from the original monolithic dispatch.js.
module.exports = (deps) => {
  const { ctx, state, store, glyphs, logger, adapters, config, res, clone, diffMantle,
          isDirty, M, T, AX, createRune, createBinding, parseRef, fs, path, spawn,
          mantleOrThrow, runeOrThrow, targets, coerce } = deps;
  return {
    async save(c) {
      logger.info('save', 'Save Progress…');
      if (typeof adapters.save === 'function') await adapters.save(ctx);  // site-sync to real files
      state._baseline = clone(state.mantles);
      store.save(state);
      ctx.baseline = clone(state.mantles);
      logger.info('save', `working state written to ${store.file}`);
      return res(['saved']);
    },
    async build(c) {
      const d = ctx.activeDomain();
      if (!d || !d.build) throw new Error('no build command on active domain');
      const code = await logger.run(d.build, [], { cwd: d.repo }, 'build');
      return res([code === 0 ? 'build ok' : 'build failed'], { code }, code === 0);
    },
    async deploy(c, pos, flags) {
      const d = ctx.activeDomain();
      if (!d) throw new Error('no active domain to deploy');
      logger.info('deploy', 'Update Website…');
      const msg = flags.message || `Update content ${new Date().toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' })}`;
      if (typeof adapters.save === 'function') await adapters.save(ctx);   // ensure real files reflect edits
      state._baseline = clone(state.mantles);
      store.save(state);
      ctx.baseline = clone(state.mantles);
      if (d.repo && fs.existsSync(path.join(d.repo, '.git'))) {
        await logger.run('git', ['add', '.'], { cwd: d.repo }, 'deploy');
        await logger.run('git', ['commit', '-m', msg], { cwd: d.repo }, 'deploy'); // ok if nothing to commit
        await logger.run('git', ['push'], { cwd: d.repo }, 'deploy');
      }
      let code = 0;
      if (d.deploy) code = await logger.run(d.deploy, [], { cwd: d.repo }, 'deploy');
      if (code === 0) logger.info('deploy', `SUCCESS${d.liveUrl ? ' — ' + d.liveUrl : ''}`);
      else logger.error('deploy', 'deploy command failed (see log)');
      return res([code === 0 ? 'deployed' : 'deploy failed'], { code }, code === 0);
    },
    preview(c, pos) {
      const d = ctx.activeDomain();
      const sub = pos[0] || 'status';
      if (sub === 'start') {
        if (!d || !d.preview) throw new Error('no preview command on active domain');
        if (ctx.preview) return res(['preview already running']);
        logger.info('preview', `starting: ${d.preview}`);
        const proc = spawn(d.preview, [], { cwd: d.repo, shell: true });
        proc.stdout.on('data', dat => dat.toString().split('\n').forEach(s => s.trim() && logger.info('preview', s.trim())));
        proc.stderr.on('data', dat => dat.toString().split('\n').forEach(s => s.trim() && logger.info('preview', s.trim())));
        proc.on('close', code => { logger.info('preview', `stopped (${code})`); ctx.preview = null; });
        ctx.preview = proc;
        return res(['preview starting…']);
      }
      if (sub === 'stop') {
        if (!ctx.preview) return res(['no preview running']);
        ctx.preview.kill();
        ctx.preview = null;
        return res(['preview stopped']);
      }
      return res([ctx.preview ? 'preview: running' : 'preview: stopped'], { running: !!ctx.preview });
    },
    revert(c) {
      state.mantles = clone(ctx.baseline);
      return res(['reverted to last save']);
    },
    async batch(c, pos, flags, dispatch) {
      const file = pos[0];
      const raw = fs.readFileSync(file, 'utf8');
      let cmds;
      try { const j = JSON.parse(raw); cmds = Array.isArray(j) ? j : j.cmds; }
      catch { cmds = raw.split('\n').map(s => s.trim()).filter(s => s && !s.startsWith('#')); }
      ctx.pushUndo(`batch ${file}`);
      const snapshot = { mantles: clone(state.mantles), active: clone(state.active) };
      try {
        for (const cmd of cmds) { const r = await dispatch(cmd); if (!r.ok) throw new Error(`step failed: ${cmd}`); }
        return res([`applied ${cmds.length} command(s)`]);
      } catch (e) {
        state.mantles = snapshot.mantles; state.active = snapshot.active;   // atomic rollback
        ctx.undoStack.pop();
        throw new Error(`batch rolled back: ${e.message}`);
      }
    },
  };
};
