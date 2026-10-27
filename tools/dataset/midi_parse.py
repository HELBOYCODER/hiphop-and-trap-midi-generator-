"""Minimal dependency-free Standard MIDI File parser (formats 0/1, running status, tempo map)."""
import struct


class MidiError(Exception):
    pass


def _vlq(data, i):
    v = 0
    for _ in range(4):
        b = data[i]
        i += 1
        v = (v << 7) | (b & 0x7F)
        if not b & 0x80:
            return v, i
    raise MidiError("bad variable-length quantity")


def parse_midi(data):
    """Return dict(ppq, tempos=[(tick, microsec_per_quarter)], tracks=[[note,...]]).

    A note is (start_tick, end_tick, pitch, velocity, channel).
    """
    if data[:4] != b"MThd":
        raise MidiError("not a MIDI file")
    hlen, fmt, ntrks, division = struct.unpack(">IHHH", data[4:14])
    if division & 0x8000:
        raise MidiError("SMPTE time division not supported")
    i = 8 + hlen
    tempos, tracks = [], []
    while i + 8 <= len(data) and len(tracks) < ntrks:
        cid, clen = data[i:i + 4], struct.unpack(">I", data[i + 4:i + 8])[0]
        body = data[i + 8:i + 8 + clen]
        i += 8 + clen
        if cid != b"MTrk":
            continue
        notes, active, tick, status, p = [], {}, 0, 0, 0
        while p < len(body):
            delta, p = _vlq(body, p)
            tick += delta
            b = body[p]
            if b == 0xFF:  # meta
                mtype = body[p + 1]
                mlen, p = _vlq(body, p + 2)
                if mtype == 0x51 and mlen == 3:
                    tempos.append((tick, int.from_bytes(body[p:p + 3], "big")))
                p += mlen
                continue
            if b in (0xF0, 0xF7):  # sysex
                mlen, p = _vlq(body, p + 1)
                p += mlen
                continue
            if b & 0x80:
                status = b
                p += 1
            kind, ch = status & 0xF0, status & 0x0F
            if kind in (0xC0, 0xD0):
                p += 1
                continue
            d1, d2 = body[p], body[p + 1]
            p += 2
            if kind == 0x90 and d2 > 0:
                active.setdefault((ch, d1), []).append((tick, d2))
            elif kind == 0x80 or (kind == 0x90 and d2 == 0):
                stack = active.get((ch, d1))
                if stack:
                    st, vel = stack.pop(0)
                    notes.append((st, max(tick, st + 1), d1, vel, ch))
        notes.sort()
        tracks.append(notes)
    if not tempos:
        tempos = [(0, 500000)]
    return {"ppq": division, "tempos": sorted(tempos), "tracks": tracks}
