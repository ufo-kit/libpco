is_installed=`rpm --quiet -q siso-rt5-devel`
if [ $is_installed -eq 0 ] ; then 
  sudo rpm -e siso-rt5-devel
fi

is_installed=`rpm --quiet -q siso-rt5`
if [ $is_installed -eq 0 ] ; then
  sudo rpm -e siso-rt5
fi

sudo rpm -i siso-*
. /etc/profile.d/siso-rt5.sh
