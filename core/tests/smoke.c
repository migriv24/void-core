/* smoke.c — exercises the public ABI end to end, Deltarune-flavored.
 * Builds a cutscene-ish mantle, edits a dialogue rune, and dumps the state. */
#include "voidcore.h"
#include <stdio.h>

static void run(VC_Manager *m, const char *cmd) {
  char *r = vc_dispatch(m, cmd);
  printf(">> %s\n%s\n\n", cmd, r ? r : "(null)");
  vc_free_str(r);
}

int main(void) {
  printf("Void Core C smoke — version %s\n\n", vc_version());

  VC_Manager *m = vc_create(NULL);

  /* an app registers its glyphs up front (the Deltarune tool will do exactly
   * this for each cutscene-action type). */
  vc_register_glyph(m,
      "{\"glyph\":\"dialogue\",\"label\":\"Dialogue line\",\"editor\":\"form\","
      "\"fields\":[\"speaker\",\"text\",\"expression\"]}");
  vc_register_glyph(m,
      "{\"glyph\":\"walk\",\"label\":\"Walk action\",\"editor\":\"form\","
      "\"fields\":[\"actor\",\"x\",\"y\",\"speed\"]}");

  run(m, "version");
  run(m, "glyphs");
  run(m, "mantle new castle-town");
  run(m, "rune new wobble bad-glyph");   /* -> unknown glyph (rejected) */
  run(m, "rune new dialogue susie-intro");
  run(m, "set susie-intro text \"Hey, Kris! What's up?\"");
  run(m, "set susie-intro speaker susie");
  run(m, "facet susie-intro what \"Susie greets Kris in Castle Town\"");
  run(m, "tag susie-intro +chapter:2 +susie +intro-scene");
  run(m, "tag susie-intro -intro-scene");
  run(m, "rune new walk kris-walk-in");
  run(m, "ls");
  run(m, "describe susie-intro");
  run(m, "get susie-intro text");
  run(m, "cat susie-intro");
  run(m, "bogus-verb foo");

  /* tag system (SPEC §5): filter grammar, glyph-as-tag, @-group targeting */
  run(m, "rune new dialogue ralsei-greet");
  run(m, "tag ralsei-greet +chapter:2 +ralsei +status:draft");
  run(m, "tag susie-intro +status:final");
  run(m, "ls --tag chapter:2");
  run(m, "ls --tag chapter:2 AND ralsei");
  run(m, "ls --tag susie OR ralsei");
  run(m, "ls --tag chapter:2 AND NOT ralsei");
  run(m, "ls --tag glyph:dialogue");          /* glyph surfaced as a tag */
  run(m, "ls --tag susie-intro&glyph:dialogue"); /* mid-word & = tag char; once crashed */
  run(m, "ls --tag susie & ralsei");             /* lone & = never-matching tag, not a hang */
  printf("tag_match(a&b) standalone = %d\n\n",
         vc_tag_match("x&y", "[\"x\",\"y\",\"x&y\"]")); /* -> 1: one atom */
  run(m, "set @chapter:2 reviewed yes");       /* multi-target write */
  run(m, "axes");
  run(m, "find greets");

  /* config verb (SPEC §7 system family): session meta, outside the undo slice */
  run(m, "config set bpm 140");
  run(m, "config get bpm");
  run(m, "config set title \"castle town demo\"");
  run(m, "config");

  /* attribution (SPEC §9): config.actor stamps who on log records + undo frames */
  run(m, "config set actor smoke-agent");
  run(m, "set susie-intro expression grin");
  run(m, "log --tail 2");     /* -> mutation spine line with (smoke-agent) */
  run(m, "history --tail 2"); /* -> frame with [smoke-agent] suffix */
  run(m, "config set actor \"\"");

  /* undo/redo (SPEC §6) */
  run(m, "set susie-intro text \"CHANGED LINE\"");
  run(m, "get susie-intro text");   /* -> CHANGED LINE */
  run(m, "history");
  run(m, "undo");
  run(m, "get susie-intro text");   /* -> back to original */
  run(m, "redo");
  run(m, "get susie-intro text");   /* -> CHANGED LINE again */
  run(m, "undo 99");                /* undo every mutation incl. mantle creation */
  run(m, "mantles");                /* -> (no mantles) */
  run(m, "undo");                   /* -> nothing to undo */
  run(m, "redo 99");                /* restore everything for the round-trip below */

  /* round-trip: export the state, rebuild a manager from it, re-list. */
  char *state = vc_export_state(m);
  printf("=== EXPORTED STATE ===\n%s\n\n", state ? state : "(null)");
  VC_Manager *m2 = vc_create(state);
  vc_free_str(state);
  run(m2, "use castle-town");
  run(m2, "ls");

  vc_destroy(m2);
  vc_destroy(m);
  printf("OK\n");
  return 0;
}
