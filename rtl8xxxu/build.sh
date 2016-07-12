sudo make clean -C /lib/modules/`uname -r`/build M=`pwd` CONFIG_RTL8XXXU=m CONFIG_RTL8XXXU_UNTESTED=y 
sudo make -C /lib/modules/`uname -r`/build M=`pwd` CONFIG_RTL8XXXU=m CONFIG_RTL8XXXU_UNTESTED=y 

