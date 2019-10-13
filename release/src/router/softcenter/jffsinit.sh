#!/bin/sh

VERSIONCMP=versioncmp

if [ ! -d /jffs/softcenter ]; then
	mkdir -p /jffs/softcenter
	cp -rf /rom/etc/softcenter/* /jffs/softcenter/
fi
mkdir -p /jffs/softcenter/init.d
mkdir -p /jffs/softcenter/bin
mkdir -p /jffs/softcenter/etc
mkdir -p /jffs/softcenter/scripts
mkdir -p /jffs/softcenter/webs
mkdir -p /jffs/softcenter/res
mkdir -p /jffs/softcenter/ss
mkdir -p /jffs/softcenter/lib
mkdir -p /jffs/configs/dnsmasq.d
mkdir -p /jffs/softcenter/configs
cp -rf /rom/etc/softcenter/scripts/* /jffs/softcenter/scripts/
cp -rf /rom/etc/softcenter/res/* /jffs/softcenter/res/
cp -rf /rom/etc/softcenter/webs/* /jffs/softcenter/webs/
cp -rf /rom/etc/softcenter/bin/* /jffs/softcenter/bin/
cp -rf /rom/etc/softcenter/perp /jffs/softcenter/
ln -sf /jffs/softcenter/bin/base64_encode /jffs/softcenter/bin/base64_decode
ln -sf /jffs/softcenter/scripts/ks_app_install.sh /jffs/softcenter/scripts/ks_app_remove.sh
ln -sf  /jffs/softcenter/bin/softcenter.sh /jffs/.asusrouter
chmod 755 /jffs/softcenter/scripts/*.sh
chmod 755 /jffs/softcenter/configs/*.sh
chmod 755 /jffs/softcenter/bin/*
chmod 755 /jffs/softcenter/init.d/*
chmod 755 /jffs/softcenter/perp/*
chmod 755 /jffs/softcenter/perp/.boot/*
chmod 755 /jffs/softcenter/perp/.control/*
echo 1.2.0 > /jffs/softcenter/.soft_ver
if [ "`nvram get model`" == "GT-AC5300" ] || [ "`nvram get model`" == "GT-AC2900" ];then
	cp -rf /rom/etc/softcenter/ROG/webs/* /jffs/softcenter/webs/
	cp -rf /rom/etc/softcenter/ROG/res/* /jffs/softcenter/res/
fi
# creat wan-start file
mkdir -p /jffs/scripts

if [ ! -f "/jffs/scripts/wan-start" ];then
	cat > /jffs/scripts/wan-start <<-EOF
	#!/bin/sh
	/jffs/softcenter/bin/softcenter-wan.sh start
	EOF
	chmod +x /jffs/scripts/wan-start
else
	STARTCOMAND1=`cat /jffs/scripts/wan-start | grep -c "/jffs/softcenter/bin/softcenter-wan.sh start"`
	[ "$STARTCOMAND1" -gt "1" ] && sed -i '/softcenter-wan.sh/d' /jffs/scripts/wan-start && sed -i '1a /jffs/softcenter/bin/softcenter-wan.sh start' /jffs/scripts/wan-start
	[ "$STARTCOMAND1" == "0" ] && sed -i '1a /jffs/softcenter/bin/softcenter-wan.sh start' /jffs/scripts/wan-start
fi

if [ ! -f "/jffs/scripts/nat-start" ];then
	cat > /jffs/scripts/nat-start <<-EOF
	#!/bin/sh
	/jffs/softcenter/bin/softcenter-net.sh start_nat
	EOF
	chmod +x /jffs/scripts/nat-start
else
	STARTCOMAND2=`cat /jffs/scripts/nat-start | grep -c "/jffs/softcenter/bin/softcenter-net.sh start"`
	[ "$STARTCOMAND2" -gt "1" ] && sed -i '/softcenter-net.sh/d' /jffs/scripts/nat-start && sed -i '1a /jffs/softcenter/bin/softcenter-net.sh start_nat' /jffs/scripts/nat-start
	[ "$STARTCOMAND2" == "0" ] && sed -i '1a /jffs/softcenter/bin/softcenter-net.sh start_nat' /jffs/scripts/nat-start
fi

if [ ! -f "/jffs/scripts/post-mount" ];then
	cat > /jffs/scripts/post-mount <<-EOF
	#!/bin/sh
	/jffs/softcenter/bin/softcenter-mount.sh start
	EOF
	chmod +x /jffs/scripts/post-mount
else
	STARTCOMAND2=`cat /jffs/scripts/post-mount | grep "/jffs/softcenter/bin/softcenter-mount.sh start"`
	[ "$STARTCOMAND2" -gt "1" ] && sed -i '/softcenter-mount.sh/d' /jffs/scripts/post-mount && sed -i '1a /jffs/softcenter/bin/softcenter-mount.sh start' /jffs/scripts/post-mount
	[ "$STARTCOMAND2" == "0" ] && sed -i '1a /jffs/softcenter/bin/softcenter-mount.sh start' /jffs/scripts/post-mount
fi

# creat profile file
if [ ! -f /jffs/configs/profile.add ]; then
cat > /jffs/configs/profile.add <<EOF
alias ls='ls -Fp --color=auto'
alias ll='ls -lFp --color=auto'
alias l='ls -AlFp --color=auto'
source /jffs/softcenter/scripts/base.sh
export PERP_BASE=/softcenter/perp

EOF
fi

