#!/usr/bin/env python3
"""
Xipher Python Bot Runner

The bot script must define either:
  - def handle(update, api): ...
  - def handle(update): return "reply text" or {"reply": "..."}

The runner reads JSON "update" from stdin and prints JSON to stdout:
  {"ok": true, "actions": [{"type": "send", "text": "..."}]}
or
  {"ok": false, "error": "..."}
"""

import json
import sys
import traceback
import importlib.util
from types import SimpleNamespace


class Api:
    def __init__(self):
        self.actions = []

    def send(self, text: str):
        self.actions.append({"type": "send", "text": str(text)})

    def send_dm(self, user_id: str, text: str):
        self.actions.append({"type": "send_dm", "user_id": str(user_id), "text": str(text)})

    def send_group(self, group_id: str, text: str):
        self.actions.append({"type": "send_group", "group_id": str(group_id), "text": str(text)})


def load_bot(path: str):
    import os
    # Add project directory to sys.path for imports
    project_dir = os.path.dirname(os.path.abspath(path))
    if project_dir not in sys.path:
        sys.path.insert(0, project_dir)
    
    spec = importlib.util.spec_from_file_location("xipher_user_bot", path)
    if spec is None or spec.loader is None:
        raise RuntimeError("Failed to load bot script")
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)  # type: ignore
    return mod


def main():
    if len(sys.argv) < 2:
        print(json.dumps({"ok": False, "error": "bot_path arg required"}))
        return 2

    bot_path = sys.argv[1]
    try:
        raw = sys.stdin.read()
        update = json.loads(raw) if raw.strip() else {}
    except Exception as e:
        print(json.dumps({"ok": False, "error": f"invalid update json: {e}"}))
        return 2

    api = Api()
    try:
        bot = load_bot(bot_path)
        fn = getattr(bot, "handle", None)
        if fn is None or not callable(fn):
            raise RuntimeError("Bot script must define callable handle(update[, api])")

        # Try (update, api) first, fallback to (update)
        try:
            res = fn(update, api)
        except TypeError:
            res = fn(update)

        if isinstance(res, str) and res.strip():
            api.send(res)
        elif isinstance(res, dict):
            txt = res.get("reply") or res.get("text")
            if isinstance(txt, str) and txt.strip():
                api.send(txt)

        print(json.dumps({"ok": True, "actions": api.actions}, ensure_ascii=False))
        return 0
    except Exception as e:
        err = "".join(traceback.format_exception_only(type(e), e)).strip()
        print(json.dumps({"ok": False, "error": err}, ensure_ascii=False))
        return 1


if __name__ == "__main__":
    raise SystemExit(main())


