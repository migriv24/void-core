'use strict';
// Domain — the base a mantle sits on: the website's real hosting target.
// See ARCHITECTURE.md §2.4. This is the seam between the abstract model and
// the actual filesystem / deploy commands.

function createDomain({ name, repo, liveUrl, build, deploy, preview, port } = {}) {
  if (!name) throw new Error('domain requires a name');
  return {
    name,
    repo: repo || null,          // absolute path to the website's repo
    liveUrl: liveUrl || null,
    build: build || null,        // shell command, e.g. "npm run build"
    deploy: deploy || null,      // shell command, e.g. "npm run deploy"
    preview: preview || null,    // shell command, e.g. "npm run dev"
    port: port || null,          // this manager's local server port
  };
}

module.exports = { createDomain };
