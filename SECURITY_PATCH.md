# Applying the focused security patch

This branch contains a safe, exact-match patch script rather than wholesale replacements of the large firmware headers.

From the repository root, first validate that the expected source fragments match:

```bash
python3 scripts/apply_security_patch.py --check
```

If preflight passes, apply the edits and inspect the resulting diff:

```bash
python3 scripts/apply_security_patch.py --apply
git diff --check
git diff -- WebServerManager.h WiFiPortal.h
```

The script creates `WebServerManager.h.security-backup` and `WiFiPortal.h.security-backup`. It aborts if the source differs from the expected version or if either backup already exists. The changes remove password logging, stop returning API keys/tokens in the configuration GET response, preserve existing tokens when the input fields are left blank, and strengthen the new web password rule.

Build using the project's actual Arduino/ESP32 board and library versions, then test login, credential changes, saving unrelated system settings, reset behavior, AP setup, and relay controls on a test device. Do not flash production hardware until those checks pass. This does not add HTTPS, CSRF protection, rate limiting, or encrypted NVS; those need separate design and testing.
