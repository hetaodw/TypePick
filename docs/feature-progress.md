# TypePick usability update

Scope approved: integrated candidate recommendations, caret context, more apps,
personal vocabulary, and local short-phrase completion.

- [x] Integrate recommendations without reordering numbered candidates.
- [x] Read bounded TSF context; suppress recommendations in sensitive fields.
- [x] Add opt-in Edge/Chrome/Word/WeChat support with safe fallback.
- [x] Persist personal candidate choices and manage custom phrases locally.
- [x] Complete phrases using Tab through the normal TSF commit path.
- [x] Settings, regression tests, full Windows CI, installer and installed checks.

Keep development confidence display enabled. Do not upload personal phrase history
as a dictionary or write document content into diagnostic logs. App compatibility
must distinguish protocol tests from tests inside actual applications.

Validation: four core test suites and six real-Rime bridge cases passed locally.
Windows CI run 35814967909 passed full TSF/server compilation, install/uninstall,
existing regressions, custom-term commit, phrase suffix commit, scope gating and
caret invalidation. Settings and vocabulary editor visually checked on this host.
Revision 1db4f59 installed locally with 43 payload hashes verified, then actual
Jev numeric recommendation and Tab commit passed through the installed service.
Final regression adds a seventh real-Rime case for local commits, unseen-result
rejection, Escape dismissal and denial persistence after a commit. Desktop helper
could not activate the independent TSF host (failed to activate captured window),
so integrated candidate rendering and real browser/Office/WeChat compatibility
remain explicitly unverified. Those host switches stay experimental and opt-in.
