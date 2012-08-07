rpm --quiet -q siso-rt5-devel
if [ $? -eq 0 ] ; then 
  sudo rpm -e siso-rt5-devel
fi

rpm --quiet -q siso-rt5
if [ $? -eq 0 ] ; then
  sudo rpm -e siso-rt5
fi

sudo rpm -i siso-*
. /etc/profile.d/siso-rt5.sh
