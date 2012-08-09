rpm --quiet -q siso-rt5-devel
if [ $? -eq 0 ] ; then 
  sudo rpm -e siso-rt5-devel
fi

rpm --quiet -q siso-rt5
if [ $? -eq 0 ] ; then
  sudo rpm -e siso-rt5
fi

sudo rpm -i siso-rt5-5.1.2-1.x86_64.rpm
sudo rpm -i siso-rt5-devel-5.1.2-1.x86_64.rpm 
. /etc/profile.d/siso-rt5.sh
