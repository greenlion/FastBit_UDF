FASTBIT_VERSION="2.0.1"
 
if [ ! -e "fastbit-2.0.1.tar.gz" ]
then
  wget https://codeforge.lbl.gov/frs/download.php/415/fastbit-"$FASTBIT_VERSION".tar.gz || exit 1
fi

if [ ! -e "lib" ]
then 

  if [ ! -e "fastbit-2.0.1" ]
  then
    tar zxovf fastbit-2.0.1.tar.gz || exit 1
  fi

  cd fastbit-2.0.1/

  if [ ! -e "Makefile" ]
  then
    ./configure --prefix=$(pwd)/.. || exit 1
  fi

  if [ "$1" eq "clean" ]
  then
    make clean || exit 1
  fi

  make -j `cat /proc/cpuinfo|grep bogomips | wc -l` || exit 1

  make install || exit 1

fi
