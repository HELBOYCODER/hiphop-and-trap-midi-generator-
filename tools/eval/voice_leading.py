# Average per-voice movement between consecutive chord bars (lower = smoother voice leading).
# usage: python3 -I tools/eval/voice_leading.py build/hhmg-cli
import json, os, subprocess, sys, statistics
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'dataset'))
from midi_parse import parse_midi
import tempfile,os
mov=[]
with tempfile.TemporaryDirectory() as d:
    for i in range(60):
        p=os.path.join(d,'x.mid')
        subprocess.run([sys.argv[1],'call','generate_midi'],input=json.dumps({"root":i%12,"scale":"minor","style":"trap","bars":4,"seed":i+1,"melody":False,"humanize":0,"complexity":0.7,"output_path":p}),capture_output=True,text=True,check=True)
        m=parse_midi(open(p,'rb').read()); notes=[n for t in m['tracks'] for n in t if n[4]==0]
        bars={}
        for n in notes: bars.setdefault(n[0]//(m['ppq']*4),[]).append(n[2])
        ks=sorted(bars)
        for a,b in zip(ks,ks[1:]):
            x,y=sorted(set(bars[a])),sorted(set(bars[b])); k=min(len(x),len(y))
            mov.append(sum(abs(x[j]-y[j]) for j in range(k))/k)
print("avg voice movement (semitones/voice):",round(statistics.mean(mov),2))
