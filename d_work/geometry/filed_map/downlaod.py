import paramiko

hostname = "enpg.cn"
username = "tbt23"
port = 14869
key_path = "/mnt/c/Users/tbt/.ssh/id_rsa"  

remote_path = "/nas/data/Bryan/field_map/180626-1,20T-3000.bin"
local_path = "180626-1,20T-3000.bin"

key = paramiko.RSAKey.from_private_key_file(key_path)

ssh = paramiko.SSHClient()
ssh.set_missing_host_key_policy(paramiko.AutoAddPolicy())
ssh.connect(hostname, port=port, username=username, pkey=key)

sftp = ssh.open_sftp()
sftp.get(remote_path, local_path)
sftp.close()
ssh.close()

print(f"文件已下载到 {local_path}")