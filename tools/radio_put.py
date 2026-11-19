import os, sys, paramiko
ip, local, remote = sys.argv[1:4]
c = paramiko.SSHClient()
c.set_missing_host_key_policy(paramiko.AutoAddPolicy())
c.connect(ip, username='root', key_filename=os.path.expanduser('~/.ssh/x6100_radio_ed25519'), timeout=15,
          look_for_keys=False, allow_agent=False)
s = c.open_sftp()
s.put(local, remote)
s.chmod(remote, 0o755)
print('put', remote)
c.close()
