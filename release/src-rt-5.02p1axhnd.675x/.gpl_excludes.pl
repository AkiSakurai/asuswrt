#!/usr/bin/perl
# $1: [model], $2: [srcbasedir]


sub append_gpl_excludes
{
	my $fexclude;
	my $uc_model;
	my $srcbasedir;

	$uc_model=uc($_[0]);
	$srcbasedir=$_[1];

	system("touch ./.gpl_excludes.sysdeps");
	open($fexclude, ">./.gpl_excludes.sysdeps");

	if ($uc_model == "RT-AX86U" || $uc_model == "RT-AX68U") {
		print $fexclude "toolchain\n";
		print $fexclude "tools\n";

		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/bcm963178\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl61\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl69\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/43602\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/4365\n";
		if ($uc_model == "RT-AX68U") {
			print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/43684\n";
		}

		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/char/cmdlist/impl1/*save*\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/components/avs/src/avs.o_saved-*\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/src/emf/linux/prebuilt\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/src/hnd/linux/prebuilt\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/src/igs/linux/prebuilt\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/src/shared/linux/*save*\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/src/wl/linux/prebuilt\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/main/tools/*save*\n";
		print $fexclude "${srcbasedir}/bcmdrivers/broadcom/net/wl/impl63/sys/src/dhd/linux/prebuilt\n";
		print $fexclude "${srcbasedir}/router-sysdep/bshared\n";
		print $fexclude "${srcbasedir}/router-sysdep/emf/emfconf/prebuilt/*save*\n";
		print $fexclude "${srcbasedir}/router-sysdep/emf/igsconf/prebuilt/*save*\n";
		print $fexclude "${srcbasedir}/router-sysdep/hnd_wl/*save*\n";
		print $fexclude "${srcbasedir}/router-sysdep/seltctl*\n";
		print $fexclude "${srcbasedir}/kernel/dts/4908/94908.dts.*\n";
		print $fexclude "${srcbasedir}/targets/cfe/sysdeps\n";
	}

	close($fexclude);
}

if (@ARGV >= 2) {
	append_gpl_excludes($ARGV[0], $ARGV[1]);
}
else {
	print "usage: .gpl_excludes.pl [model] [srcbasedir]\n";
}

