# Agent-native use / استفادهٔ ایجنت‌محور

**EN.** The whole engine runs headless. An AI agent can use it three ways:
1. `hhmg-cli list | describe <tool> | call <tool> --args '{…}'` — JSON out, exit code 0 = ok.
2. `hhmg-cli mcp` — the same tools as a stdio MCP server (Claude Desktop, Cursor, …).
3. Skill: copy `skills/hhmg/SKILL.md` into your agent's skills folder.

Inside the apps (VST3 and Android) the AI is connected to the same tools **and** to external MCP sites. Hugging Face's public MCP server is connected by default (dataset / model search, no key); add any streamable-HTTP MCP endpoint in the "MCP" menu (plugin) or card (Android). The AI can search there for MIDI datasets, styles, lyrics or poems, then call `lyrics_to_midi`, `generate_midi` or `similar_to_current_clip` on what it found.

**Turning other programs or sites into agent tools.** This repo does not wrap arbitrary GUIs automatically. The pattern (as in the CLI-Anything idea) is: find the program's scripting/automation/import-export surface (or a site's API), describe each action as a JSON-schema tool, expose it via a CLI + MCP, and ship a SKILL.md. `tools/typebeats/harvest.py` is a worked example for a website (public listing metadata → aggregate style bank).

**FA.** کل موتور بدون رابط گرافیکی کار می‌کند: CLI با خروجی JSON، سرور MCP (`hhmg-cli mcp`) و فایل مهارت `skills/hhmg/SKILL.md`. داخل اپ‌ها (پلاگین و اندروید) هوش مصنوعی هم به همین ابزارها و هم به سایت‌های MCP بیرونی وصل است؛ سرور MCP رسمی Hugging Face به‌صورت پیش‌فرض متصل است (جستجوی دیتاست/مدل، بدون کلید). هر آدرس MCP دیگری را می‌توانید اضافه کنید. تبدیل خودکار هر برنامهٔ گرافیکی/سایت به API در این پروژه انجام نمی‌شود؛ الگو همین است: سطح اسکریپت/API برنامه را پیدا کن، هر عمل را ابزار JSON کن، با CLI و MCP عرضه کن و SKILL.md بنویس.
