#!/bin/sh

source /jffs/softcenter/scripts/base.sh
eval $(dbus export soft)
alias echo_date='echo 【$(date +%Y年%m月%d日\ %X)】:'
if [ -n "$SCAPI" ];then
	TARGET_DIR=/tmp/upload
else
	TARGET_DIR=/tmp
fi
MODEL=$(nvram get productid)
if [ "$MODEL" == "GT-AC5300" ] || [ "$MODEL" == "GT-AC2900" ] || [ "$MODEL" == "GT-AX11000" ] || [ "$(nvram get merlinr_rog)" == "1" ];then
	ROG=1
elif [ "$MODEL" == "TUF-AX3000" ] || [ "$(nvram get merlinr_tuf)" == "1" ];then
	TUF=1
fi
clean(){
	[ -n "$name" ] && rm -rf /tmp/$name >/dev/null 2>&1
	[ -n "$MODULE_NAME" ] && rm -rf /tmp/$MODULE_NAME >/dev/null 2>&1
	[ -n "$soft_name" ] && rm -rf /tmp/$soft_name >/dev/null 2>&1
	rm -rf /tmp/*.tar.gz >/dev/null 2>&1
	dbus remove soft_install_version
	dbus remove soft_name
}

detect_package(){
	local TEST="$1"
	local KEYWORDS="ss|ssr|shadowsocks|shadowsocksr|v2ray|trojan|clash|wireguard|koolss|brook"
	local KEY_MATCH=$(echo "${TEST}" | grep -Eo "$KEYWORDS")
	if [ -n "$KEY_MATCH" ]; then
		echo_date =======================================================
		echo_date "检测到离线安装包：${soft_name} 含非法关键词！！！"
		echo_date "根据法律规定，软件中心将不会安装此插件！！！"
		echo_date "删除相关文件并退出..."
		echo_date =======================================================
		clean
		exit 1
	fi
}
install_tar(){
	#do the right thing
	detect_package "$soft_name"
	
	name=$(echo "$soft_name"|sed 's/.tar.gz//g'|awk -F "_" '{print $1}'|awk -F "-" '{print $1}')
	INSTALL_SUFFIX=_install
	VER_SUFFIX=_version
	NAME_SUFFIX=_name
	cd /tmp
	echo_date ====================== step 1 ===========================
	echo_date 开启软件离线安装！
	if [ -f $TARGET_DIR/$soft_name ];then
		local _SIZE=$(ls -lh $TARGET_DIR/$soft_name|awk '{print $5}')
		echo_date $TARGET_DIR目录下检测到上传的离线安装包$soft_name，大小：$_SIZE
		[ -n "$SCAPI" ] && mv /tmp/upload/$soft_name /tmp
		echo_date 尝试解压离线安装包离线安装包
		tar -zxvf $soft_name >/dev/null 2>&1
		if [ "$?" == "0" ];then
			echo_date 解压完成！
			cd /tmp
		else
			echo_date 解压错误，错误代码："$?"！
			echo_date 估计是错误或者不完整的的离线安装包！
			echo_date 删除相关文件并退出...
			clean
			dbus remove "softcenter_module_$MODULE_NAME$INSTALL_SUFFIX"
			echo_date ======================== end ============================
			echo XU6J03M6
			exit
		fi
		
		if [ -f /tmp/$name/install.sh ];then
			INSTALL_SCRIPT=/tmp/$name/install.sh
		else
			INSTALL_SCRIPT_NU=$(find /tmp -name "install.sh"|wc -l) 2>/dev/null
			[ "$INSTALL_SCRIPT_NU" == "1" ] && INSTALL_SCRIPT=$(find /tmp -name "install.sh") || INSTALL_SCRIPT=""
		fi

		if [ -n "$INSTALL_SCRIPT" -a -f "$INSTALL_SCRIPT" ];then
			SCRIPT_AB_DIR=$(dirname $INSTALL_SCRIPT)
			MODULE_NAME=${SCRIPT_AB_DIR##*/}
			echo_date 准备安装$MODULE_NAME插件！
			echo_date 找到安装脚本！
			chmod +x $INSTALL_SCRIPT >/dev/null 2>&1
			echo_date 运行安装脚本...
			echo_date ====================== step 2 ===========================

			if [ -d /tmp/$MODULE_NAME/ROG -a "$ROG" == "1" ]; then
				echo_date "检测到ROG皮肤，安装中..."
				cp -rf /tmp/$MODULE_NAME/ROG/* /tmp/$MODULE_NAME/
			elif [ -d /tmp/${MODULE_NAME}/ROG -a "$TUF" == "1" ]; then
				# 骚红变橙色
				echo_date "检测到TUF皮肤，安装中..."
				find /tmp/${MODULE_NAME}/ROG/ -name "*.asp" | xargs sed -i 's/3e030d/3e2902/g;s/91071f/92650F/g;s/680516/D0982C/g;s/cf0a2c/c58813/g;s/700618/74500b/g;s/530412/92650F/g'
				find /tmp/${MODULE_NAME}/ROG/ -name "*.css" | xargs sed -i 's/3e030d/3e2902/g;s/91071f/92650F/g;s/680516/D0982C/g;s/cf0a2c/c58813/g;s/700618/74500b/g;s/530412/92650F/g'
				cp -rf /tmp/${MODULE_NAME}/ROG/* /tmp/${MODULE_NAME}/
			fi
			sleep 1
			start-stop-daemon -S -q -x $INSTALL_SCRIPT 2>&1
			if [ "$?" != "0" ];then
				echo_date 因为${MODULE_NAME}插件安装失败！退出离线安装！
				clean
				dbus remove "softcenter_module_${MODULE_NAME}${INSTALL_SUFFIX}"
				echo_date ======================== end ============================
				echo XU6J03M6
				exit
			fi
			echo_date ====================== step 3 ===========================
			dbus set "softcenter_module_${MODULE_NAME}${NAME_SUFFIX}=${MODULE_NAME}"
			dbus set "softcenter_module_${MODULE_NAME}${INSTALL_SUFFIX}=1"
			if [ -n "$soft_install_version" ];then
				dbus set "softcenter_module_${MODULE_NAME}${VER_SUFFIX}=$soft_install_version"
				echo_date "从插件文件名中获取到了版本号：$soft_install_version"
			else
				if [ -z "$(dbus get softcenter_module_${MODULE_NAME}${VER_SUFFIX})" ];then
					dbus set "softcenter_module_${MODULE_NAME}${VER_SUFFIX}=0.1"
					echo_date "插件安装脚本里没有找到版本号，设置默认版本号为0.1"
				else
					echo_date "插件安装脚本已经设置了插件版本号为：$(dbus get softcenter_module_${MODULE_NAME}${VER_SUFFIX})"
				fi
			fi
			install_pid=$(ps | grep -w install.sh | grep -v grep | awk '{print $1}')
			i=120
			until [ -z "$install_pid" ]
			do
				install_pid=$(ps | grep -w install.sh | grep -v grep | awk '{print $1}')
				i=$(($i-1))
				if [ "$i" -lt 1 ];then
					echo_date "Could not load nat rules!"
					echo_date 安装似乎出了点问题，请手动重启路由器后重新尝试...
					echo_date 删除相关文件并退出...
					sleep 1
					clean
					dbus remove "softcenter_module_${MODULE_NAME}${INSTALL_SUFFIX}"
					echo_date ======================== end ============================
					echo XU6J03M6
					exit
				fi
				sleep 1
			done
			echo_date 离线包安装完成！
			echo_date 一点点清理工作...
			clean
			echo_date 完成！离线安装插件成功，现在你可以退出本页面~
		else
			echo_date 没有找到安装脚本！
			echo_date 删除相关文件并退出...
			clean
		fi
	else
		echo_date 没有找到离线安装包！
		echo_date 删除相关文件并退出...
		clean
	fi
	clean
	echo_date ======================== end ============================
	echo XU6J03M6
}
if [ -z "$SCAPI" ];then
	echo " " > /tmp/syscmd.log
	install_tar > /tmp/syscmd.log
else
	echo " " > /tmp/upload/soft_log.txt
	http_response "$1"
	install_tar > /tmp/upload/soft_log.txt
fi
