# TypePick usability update

Scope approved: integrated candidate recommendations, caret context, more apps,
personal vocabulary, and local short-phrase completion.

- [x] Integrate recommendations without reordering numbered candidates.
- [x] Read bounded TSF context; suppress recommendations in sensitive fields.
- [x] Add opt-in Edge/Chrome/Word/WeChat support with safe fallback.
- [x] Persist personal candidate choices and manage custom phrases locally.
- [x] Complete phrases using Tab through the normal TSF commit path.
- [ ] Settings, regression tests, full Windows CI, installer and installed checks.

Keep development confidence display enabled. Do not upload personal phrase history
as a dictionary or write document content into diagnostic logs. App compatibility
must distinguish protocol tests from tests inside actual applications.

Validation: four core test suites and six real-Rime bridge cases passed locally.
Windows CI run 35814967909 passed full TSF/server compilation, install/uninstall,
existing regressions, custom-term commit, phrase suffix commit, scope gating and
caret invalidation. Settings and vocabulary editor visually checked on this host.
Actual TSF UI and upgraded installation still pending.
