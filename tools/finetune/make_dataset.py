#!/usr/bin/env python3
"""Build a supervised fine-tuning set (English + Persian) that teaches an LLM this plugin's vocabulary:
which scale / tempo / complexity / swing / progression produce which sound, how to call generate_midi,
and how to explain the choice and give tuning tips to the producer.

Source of truth is the engine itself (`hhmg-cli call style_guide`), so the data never drifts from the plugin.

    python3 make_dataset.py --cli ../../build/hhmg-cli --format gemini --out hhmg_tuning.jsonl
    python3 make_dataset.py --cli ../../build/hhmg-cli --format openai --n 3000 --out hhmg_openai.jsonl

Formats: gemini  -> Vertex AI / Gemini supervised tuning ({"systemInstruction","contents":[...]})
         openai  -> chat fine-tuning ({"messages":[...]}), also fine for most open-source SFT tooling

The target answer is the tool call arguments (JSON) followed by the guidance text, so the tuned model both
selects the right settings and explains them. This script only prepares data; the tuning job itself runs in
your own Google Cloud / provider account.
"""
import argparse
import json
import random
import subprocess

NOTES = ["C", "C#", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B"]
SYSTEM = ("You are the composition guide inside a MIDI plugin for R&B, trap and hip-hop producers. "
          "Choose generate_midi arguments that fit the request and explain the choice briefly in the user's language.")

EN_TEMPLATES = [
    "{label} in {key}", "make me a {label} idea", "I want {label} vibes, {key} {scale_hint}",
    "{label}, about {bpm} bpm", "give me something {label}-ish in {key}, {extra}", "{label} loop, {extra}",
    "I need a {label} beat starter in {key} at {bpm} BPM", "{label} {extra}",
]
FA_TEMPLATES = [
    "یه {label} در گام {key} می‌خوام", "{label} با {bpm} بی‌پی‌ام", "برام یه ایده‌ی {label} بساز، {extra}",
    "حس {label} می‌خوام، تونالیته {key}", "{label}، {extra}", "یه لوپ {label} در {key} بده",
]
EXTRAS = {
    "en": {"none": "", "simple": "keep it simple", "complex": "make it very jazzy and complex", "tight": "keep it tight",
           "loose": "make it loose and human", "no808": "no 808", "808": "with a heavy 808"},
    "fa": {"none": "", "simple": "ساده باشه", "complex": "خیلی پیچیده و جَزی", "tight": "تایت و دقیق",
           "loose": "شل و انسانی", "no808": "بدون ۸۰۸", "808": "با ۸۰۸ سنگین"},
}


def run_cli(cli):
    out = subprocess.run([cli, "call", "style_guide"], input='{"all":true}', capture_output=True, text=True, check=True)
    return json.loads(out.stdout)


def clamp(v, lo, hi):
    return max(lo, min(hi, v))


def make_example(rng, rec, scales, lang):
    root = rng.choice(NOTES)
    bpm = rng.randint(int(rec["bpm_range"].split("-")[0]), int(rec["bpm_range"].split("-")[1]))
    extra_key = rng.choice(list(EXTRAS[lang]))
    args = {"root": root, "scale": rec["scale"], "bpm": bpm, "bars": rec["bars"], "style_mix": rec["style_mix"],
            "complexity": rec["complexity"], "humanize": rec["humanize"], "swing": rec["swing"],
            "velocity_variation": rec["velocity_variation"], "chords": True, "melody": True, "bass": rec["bass"],
            "progression": rec["progression"]}
    if extra_key == "simple":
        args["complexity"] = round(clamp(rec["complexity"] - 0.3, 0.05, 1), 2)
    elif extra_key == "complex":
        args["complexity"] = round(clamp(rec["complexity"] + 0.35, 0, 1), 2)
    elif extra_key == "tight":
        args["humanize"], args["swing"] = round(rec["humanize"] * 0.3, 2), round(rec["swing"] * 0.3, 2)
    elif extra_key == "loose":
        args["humanize"], args["swing"] = round(clamp(rec["humanize"] + 0.3, 0, 1), 2), round(clamp(rec["swing"] + 0.2, 0, 1), 2)
    elif extra_key == "no808":
        args["bass"] = False
    elif extra_key == "808":
        args["bass"] = True
    explicit_bpm = rng.random() < 0.5
    tpl = rng.choice(EN_TEMPLATES if lang == "en" else FA_TEMPLATES)
    prompt = tpl.format(label=rec["label"] if lang == "en" else rec["label_fa"], key=root, bpm=bpm, extra=EXTRAS[lang][extra_key],
                        scale_hint="").strip(" ,")
    if not explicit_bpm and "{bpm}" not in tpl:
        args["bpm"] = (int(rec["bpm_range"].split("-")[0]) + int(rec["bpm_range"].split("-")[1])) // 2
    why = rec["why"] if lang == "en" else rec["why_fa"]
    tips_en = [f"Key: {root} suits this; any root works, lower roots make heavier 808s.",
               "Press GENERATE for new ideas; keep the Seed number to recall one.",
               "More Complexity = richer chords; more Swing/Humanize = looser feel."]
    tips_fa = [f"تونالیته‌ی {root} مناسب است؛ هر ریشه‌ای جواب می‌دهد و ریشه‌های پایین‌تر ۸۰۸ سنگین‌تر می‌سازند.",
               "برای ایده‌ی تازه GENERATE را بزنید؛ عدد Seed را نگه دارید تا ایده را برگردانید.",
               "Complexity بیشتر = آکوردهای غنی‌تر؛ Swing/Humanize بیشتر = حس شل‌تر."]
    header = (f"{root} {args['scale']}, {args['bpm']} BPM — " if lang == "en" else f"{root} {args['scale']}، {args['bpm']} BPM — ")
    answer = ("generate_midi " + json.dumps(args, ensure_ascii=False) + "\n\n" + header + why + "\n- " +
              "\n- ".join(tips_en if lang == "en" else tips_fa))
    return prompt, answer


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--cli", required=True, help="path to hhmg-cli")
    ap.add_argument("--format", choices=["gemini", "openai"], default="gemini")
    ap.add_argument("--n", type=int, default=1200, help="number of examples")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--out", default="hhmg_tuning.jsonl")
    a = ap.parse_args()

    rng = random.Random(a.seed)
    guide = run_cli(a.cli)
    recs = guide["matches"]
    scales = guide["scale_character"]
    with open(a.out, "w", encoding="utf-8") as f:
        for i in range(a.n):
            rec, lang = rng.choice(recs), "fa" if i % 3 == 0 else "en"
            prompt, answer = make_example(rng, rec, scales, lang)
            if a.format == "gemini":
                row = {"systemInstruction": {"role": "system", "parts": [{"text": SYSTEM}]},
                       "contents": [{"role": "user", "parts": [{"text": prompt}]},
                                    {"role": "model", "parts": [{"text": answer}]}]}
            else:
                row = {"messages": [{"role": "system", "content": SYSTEM}, {"role": "user", "content": prompt},
                                    {"role": "assistant", "content": answer}]}
            f.write(json.dumps(row, ensure_ascii=False) + "\n")
    print(f"{a.n} examples ({len(recs)} recipes, EN+FA) -> {a.out}")


if __name__ == "__main__":
    main()
