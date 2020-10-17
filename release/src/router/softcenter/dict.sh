#!/bin/bash

model=$1
modelname=$2
path=$3
if [ "$modelname" == "R7900P" -o  "$modelname" == "R8000P" ];then
	modelname="R8000P"
	rm -rf ${path}/images/Model_product.png
	rm -rf ${path}/images/arris.png
	rm -rf ${path}/images/New_ui/logo_ROG.png
	rm -rf ${path}/images/New_ui/icon_titleName.png
	rm -rf ${path}/images/New_ui/GT-bg_header.png
	rm -rf ${path}/images/New_ui/ROG-logo.png
	mv ${path}/images/r7900p.png ${path}/images/Model_product.png
	mv ${path}/images/New_ui/r7900plogo.png ${path}/images/New_ui/logo_ROG.png
	cp ${path}/images/New_ui/r7900ptitle.png ${path}/images/New_ui/icon_titleName.png
	mv ${path}/images/New_ui/r7900pROG-logo.png ${path}/images/New_ui/ROG-logo.png
	mv ${path}/images/New_ui/r7900pGT-bg_header.png ${path}/images/New_ui/GT-bg_header.png
elif [ "$modelname" == "SBRAC1900P" -o "$modelname" == "SBRAC3200P" ];then
	rm -rf ${path}/images/Model_product.png
	mv ${path}/images/arris.png ${path}/images/Model_product.png
	rm -rf ${path}/images/r7900p.png
fi
#cd ${path}
#for name in $(find -name "*.dict")
#do
	#sed -i "s/$model/$modelname/g" $name
#done

