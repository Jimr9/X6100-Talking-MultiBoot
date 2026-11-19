"""Fetch the RHVoice prompts from the radio, make them 48 kHz stereo 16-bit, into TALK/ (replacing the old ones)"""
import io, os, sys, wave, array, paramiko
ip = sys.argv[1]
here = os.path.dirname(os.path.abspath(__file__))
out = os.path.join(here, "TALK")
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(ip, username='root', key_filename=os.path.expanduser('~/.ssh/x6100_radio_ed25519'), timeout=15,
          look_for_keys=False, allow_agent=False)
s = c.open_sftp()
for name in sorted(s.listdir('/tmp/rhv')):
    if not name.endswith('.WAV'):
        continue
    buf = io.BytesIO()
    s.getfo('/tmp/rhv/' + name, buf)
    buf.seek(0)
    w = wave.open(buf)
    ch, width, rate, n = w.getnchannels(), w.getsampwidth(), w.getframerate(), w.getnframes()
    assert width == 2, (name, width)
    a = array.array('h', w.readframes(n))
    mono = a if ch == 1 else array.array('h', (sum(a[i:i + ch]) // ch for i in range(0, len(a), ch)))
    o = array.array('h')
    # to 48 kHz by linear steps between samples, both channels the same
    m = len(mono)
    total = m * 48000 // rate
    for j in range(total):
        x = j * rate / 48000
        i = int(x)
        f = x - i
        v = mono[i] if i + 1 >= m else int(mono[i] + (mono[i + 1] - mono[i]) * f)
        o.append(v)
        o.append(v)
    with wave.open(os.path.join(out, name), 'wb') as d:
        d.setnchannels(2)
        d.setsampwidth(2)
        d.setframerate(48000)
        d.writeframes(o.tobytes())
    print(name, rate, 'Hz', ch, 'ch ->', round(total / 48000, 1), 's')
c.close()
