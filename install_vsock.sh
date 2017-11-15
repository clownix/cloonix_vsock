#!/bin/bash
HERE=`pwd`

if [ ! -e ${HERE}/vsock_cli ] || [ ! -e ${HERE}/vsock_srv ]; then
  echo "run ./doitall to compile"
  exit 1
fi

n=$(grep -c vhost_vsock /etc/modules-load.d/modules.conf)
if (( $n == 0 )); then
  echo "vhost_vsock" >> /etc/modules-load.d/modules.conf
fi

cp -rf ${HERE}/vsock_cli /usr/bin
cp -rf ${HERE}/vsock_srv /usr/bin

cat > /etc/systemd/system/vsock_srv.service << "EOF"
[Unit]
Description=virtio-vsock host/guest use for distant bash
After=systemd-modules-load.service

[Service]
Type=forking
ExecStart=/usr/bin/vsock_srv 7777
StandardOutput=null
Restart=always

[Install]
WantedBy=multi-user.target
EOF

cd /etc/systemd/system/multi-user.target.wants
ln -s /etc/systemd/system/vsock_srv.service vsock_srv.service

