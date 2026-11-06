#!/usr/bin/env python3
"""Apply narrowly scoped security edits to the ESP32 firmware sources.

Run from the repository root:
  python3 scripts/apply_security_patch.py --check
  python3 scripts/apply_security_patch.py --apply

The script edits only exact, expected source fragments. It aborts without writing
if any fragment is missing or ambiguous, and creates .security-backup copies
before applying changes. Review the diff and build/test before flashing hardware.
"""
from __future__ import annotations

import argparse
import shutil
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

REPLACEMENTS = {
    "WebServerManager.h": [
        (
            '            Serial.printf("🔐 MẬT KHẨU WEB LẦN ĐẦU -> user: %s | pass: %s\\n", admin_user.c_str(), admin_pass.c_str());',
            '            Serial.println(F("🔐 Mật khẩu web lần đầu đã được sinh và lưu trong NVS."));',
        ),
        (
            '        doc["ts_key"] = config.getThingSpeakAPIKey();\n        doc["mcp_token"] = config.getMcpToken();',
            '        // Never return credential material to the browser.\n'
            '        doc["ts_key_configured"] = (strlen(config.getThingSpeakAPIKey()) > 0);\n'
            '        doc["mcp_token_configured"] = (strlen(config.getMcpToken()) > 0);',
        ),
        (
            '                document.getElementById(\'sysTsKey\').value=d.ts_key || ""; \n'
            '                document.getElementById(\'sysMcpToken\').value=d.mcp_token || "";\n',
            '                // Secrets are deliberately not returned by /api/sysconfig.\n'
            '                document.getElementById(\'sysTsKey\').value="";\n'
            '                document.getElementById(\'sysMcpToken\').value="";\n',
        ),
        (
            '                ts_key: document.getElementById(\'sysTsKey\').value, mcp_token: document.getElementById(\'sysMcpToken\').value,\n',
            '',
        ),
        (
            '            };\n'
            '            post(\'/api/sysconfig\', data).then(()=>alert(translations[currentLang].msg_save_ok));',
            '            };\n'
            '            const tsKey = document.getElementById(\'sysTsKey\').value.trim();\n'
            '            const mcpToken = document.getElementById(\'sysMcpToken\').value.trim();\n'
            '            if (tsKey) data.ts_key = tsKey;\n'
            '            if (mcpToken) data.mcp_token = mcpToken;\n'
            '            post(\'/api/sysconfig\', data).then(()=>alert(translations[currentLang].msg_save_ok));',
        ),
        (
            '            admin_pass = randomToken(10);',
            '            admin_pass = randomToken(20);',
        ),
        (
            '        if (incomingNewUser.length() > 32 || incomingNewPass.length() > 64) {\n'
            '            mainServer->send(400, "application/json", "{\\"status\\":\\"error\\",\\"msg\\":\\"User tối đa 32, Pass tối đa 64 ký tự\\"}");\n'
            '            return;\n'
            '        }\n'
            '        if (incomingNewPass.length() < 8) {\n'
            '            mainServer->send(400, "application/json", "{\\"status\\":\\"error\\",\\"msg\\":\\"Mật khẩu mới phải từ 8 ký tự trở lên\\"}");\n'
            '            return;\n'
            '        }',
            '        incomingNewUser.trim();\n'
            '        incomingNewPass.trim();\n'
            '        if (incomingNewUser.length() < 3 || incomingNewUser.length() > 32 || incomingNewPass.length() > 64) {\n'
            '            mainServer->send(400, "application/json", "{\\"status\\":\\"error\\",\\"msg\\":\\"User phải từ 3 đến 32 ký tự; mật khẩu tối đa 64 ký tự\\"}");\n'
            '            return;\n'
            '        }\n'
            '        bool hasLetter = false, hasDigit = false;\n'
            '        for (size_t i = 0; i < incomingNewPass.length(); ++i) {\n'
            '            const char c = incomingNewPass.charAt(i);\n'
            '            if ((c >= \'A\' && c <= \'Z\') || (c >= \'a\' && c <= \'z\')) hasLetter = true;\n'
            '            if (c >= \'0\' && c <= \'9\') hasDigit = true;\n'
            '        }\n'
            '        if (incomingNewPass.length() < 12 || !hasLetter || !hasDigit || incomingNewPass == "123456" || incomingNewPass == "12345678" || incomingNewPass == "password123") {\n'
            '            mainServer->send(400, "application/json", "{\\"status\\":\\"error\\",\\"msg\\":\\"Mật khẩu cần ít nhất 12 ký tự, gồm chữ cái và chữ số; không dùng mật khẩu phổ biến\\"}");\n'
            '            return;\n'
            '        }',
        ),
    ],
    "WiFiPortal.h": [
        (
            '        Serial.printf("🔑 Mật khẩu AP: %s\\n", apPass_.c_str());',
            '        Serial.println(F("🔐 Mật khẩu AP không được ghi vào Serial."));',
        ),
    ],
}


def plan() -> dict[Path, str]:
    planned: dict[Path, str] = {}
    problems: list[str] = []
    for filename, edits in REPLACEMENTS.items():
        path = ROOT / filename
        original = path.read_text(encoding="utf-8")
        updated = original
        for index, (old, new) in enumerate(edits, start=1):
            count = updated.count(old)
            if count != 1:
                problems.append(f"{filename}: edit {index} expected one exact match, found {count}")
                continue
            updated = updated.replace(old, new, 1)
        planned[path] = updated
    if problems:
        raise RuntimeError("Refusing partial patch:\n- " + "\n- ".join(problems))
    return planned


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--check", action="store_true", help="validate source matches; do not write")
    mode.add_argument("--apply", action="store_true", help="backup and apply exact edits")
    args = parser.parse_args()

    try:
        planned = plan()
    except (OSError, UnicodeError, RuntimeError) as exc:
        print(exc, file=sys.stderr)
        return 1

    if args.check:
        print("Patch preflight passed. No files changed.")
        return 0

    for path, content in planned.items():
        backup = path.with_name(path.name + ".security-backup")
        if backup.exists():
            print(f"Refusing to overwrite existing backup: {backup}", file=sys.stderr)
            return 1
    for path, content in planned.items():
        backup = path.with_name(path.name + ".security-backup")
        shutil.copy2(path, backup)
        path.write_text(content, encoding="utf-8", newline="")
        print(f"Updated {path.relative_to(ROOT)} (backup: {backup.name})")
    print("Review `git diff`, compile, and test before flashing the ESP32.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
