# TypePick usability update

Scope approved: integrated candidate recommendations, caret context, more apps,
personal vocabulary, and local short-phrase completion.

- [ ] Integrate recommendations without reordering numbered candidates.
- [ ] Read bounded TSF context; suppress recommendations in sensitive fields.
- [ ] Add opt-in Edge/Chrome/Word/WeChat support with safe fallback.
- [ ] Persist personal candidate choices and manage custom phrases locally.
- [ ] Complete phrases using Tab through the normal TSF commit path.
- [ ] Settings, regression tests, full Windows CI, installer and installed checks.

Keep development confidence display enabled. Do not upload personal phrase history
as a dictionary or write document content into diagnostic logs. App compatibility
must distinguish protocol tests from tests inside actual applications.
