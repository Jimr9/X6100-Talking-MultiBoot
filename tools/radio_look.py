import sys
import paramiko

host = sys.argv[1]
cmd = sys.argv[2] if len(sys.argv) > 2 else (
    "uname -a; uptime; pidof x6100_gui; ls -la /usr/sbin/x6100_gui; "
    "echo '== voice.log lines'; wc -l /tmp/voice.log 2>/dev/null; echo '== last 80'; tail -80 /tmp/voice.log 2>/dev/null"
)
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
import os
c.connect(host, username="root", key_filename=os.path.expanduser("~/.ssh/x6100_radio_ed25519"), timeout=10,
          look_for_keys=False, allow_agent=False)
_, out, err = c.exec_command(cmd, timeout=30)
sys.stdout.write(out.read().decode(errors="replace"))
sys.stdout.write(err.read().decode(errors="replace"))
c.close()
