#!/usr/bin/env python3
"""Download MIDI files (or zip packs containing them) from a Telegram post / channel you can access.

The public t.me web preview does not expose attached files, so this uses *your own* Telegram login
via Telethon (MTProto). Get api_id / api_hash at https://my.telegram.org -> API development tools.

    pip install telethon
    export TG_API_ID=123456 TG_API_HASH=abcdef...
    # the post from the brief (t.me/pulseestore/2019). A "single" link can be an album: --album fetches siblings
    python3 telegram_fetch.py --channel pulseestore --message 2019 --album --out ../../data/telegram
    # or scan a range of posts
    python3 telegram_fetch.py --channel pulseestore --from-id 2000 --to-id 2030 --out ../../data/telegram

Then:  python3 ingest.py ../../data/telegram --out patterns.json

Only download content you are entitled to use. Files are analysed locally; ingest.py stores abstract
statistics (chord-degree loops, onset grids, short contours), not the original MIDI notes, and the
data/telegram folder is git-ignored so third-party packs are never committed or redistributed.
"""
import argparse
import asyncio
import os
import sys

WANTED = (".mid", ".midi", ".zip")


async def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--channel", required=True, help="username (pulseestore) or numeric id")
    ap.add_argument("--message", type=int, help="single message id")
    ap.add_argument("--album", action="store_true", help="also fetch the neighbouring messages of the same album")
    ap.add_argument("--from-id", type=int)
    ap.add_argument("--to-id", type=int)
    ap.add_argument("--out", default="telegram_downloads")
    ap.add_argument("--session", default=os.path.expanduser("~/.hhmg_telegram"), help="session file (kept private)")
    args = ap.parse_args()

    try:
        from telethon import TelegramClient
    except ImportError:
        sys.exit("pip install telethon  (see the docstring for credentials)")
    api_id, api_hash = os.environ.get("TG_API_ID"), os.environ.get("TG_API_HASH")
    if not api_id or not api_hash:
        sys.exit("Set TG_API_ID and TG_API_HASH (https://my.telegram.org)")

    if args.message:
        ids = list(range(args.message - (9 if args.album else 0), args.message + (9 if args.album else 0) + 1))
    elif args.from_id and args.to_id:
        ids = list(range(args.from_id, args.to_id + 1))
    else:
        sys.exit("Give --message, or --from-id and --to-id")

    os.makedirs(args.out, exist_ok=True)
    async with TelegramClient(args.session, int(api_id), api_hash) as client:  # asks for phone/code on first run
        entity = await client.get_entity(args.channel)
        target_group = None
        saved = 0
        async for msg in client.iter_messages(entity, ids=ids):
            if msg is None or not msg.file:
                continue
            if args.message and args.album:
                if msg.id == args.message:
                    target_group = msg.grouped_id
                elif target_group is None or msg.grouped_id != target_group:
                    continue
            name = msg.file.name or f"{msg.id}{msg.file.ext or ''}"
            if not name.lower().endswith(WANTED):
                continue
            dest = os.path.join(args.out, f"{msg.id}_{os.path.basename(name)}")
            if os.path.exists(dest):
                continue
            print("downloading", name)
            await client.download_media(msg, file=dest)
            saved += 1
        print(f"done: {saved} file(s) in {args.out}")
        if saved == 0:
            print("nothing matched; check the message id, or that your account can see the post", file=sys.stderr)


if __name__ == "__main__":
    asyncio.run(main())
