FASTBIT_REMOTE_DIR=0
FASTBIT_VERSION=$1
if [ "" = "$FASTBIT_VERSION" ]
then
  FASTBIT_VERSION="svn"
fi

CURDIR=$(pwd)

##YOU SHOULD NOT NEED TO MODIFY BELOW HERE (unless you are using version > 2.0.1)
########################################################################################################
if [ "svn" = "$FASTBIT_VERSION" ]
then 
  echo "Getting fastbit from svn"
  svn co https://codeforge.lbl.gov/anonscm/fastbit/ || exit 1
  FASTBIT_DIR="fastbit/trunk"
else
  # Note that there are bugs in version <=2.0.1 which prevent UDF from working fully/correctly
  # Using SVN is highly recommended
  if [ "2.0.1" = "$FASTBIT_VERSION" ] 
  then
    FASTBIT_PREFIX="fastbit-"
    FASTBIT_REMOTE_DIR=415
    FASTBIT_DIR="$FASTBIT_PREFIX-$FASTBIT_VERSION"
  fi
  
  if [ "2.0.0" = "$FASTBIT_VERSION" ] 
  then
    FASTBIT_PREFIX="fastbit-"
    FASTBIT_REMOTE_DIR=414
  fi
  
  if [ "1.3.9" = "$FASTBIT_VERSION" ]
  then
    FASTBIT_PREFIX="fastbit-ibis"
    FASTBIT_REMOTE_DIR=413
  fi
  
  if [ "0" = "$FASTBIT_REMOTE_DIR" ]
  then
    echo "unsupported version of FastBit specified.  Please use 1.3.9 or higher"
    exit 1
  fi

  FASTBIT_DIR="$FASTBIT_PREFIX$FASTBIT_VERSION"
  FASTBIT_TARBALL="$FASTBIT_PREFIX$FASTBIT_VERSION.tar.gz"
  FASTBIT_URL="https://codeforge.lbl.gov/frs/download.php/$FASTBIT_REMOTE_DIR/$FASTBIT_TARBALL"
  echo $FASTBIT_VERSION $FASTBIT_URL $FASTBIT_REMOTE_DIR $FASTBIT_DIR $FASTBIT_TARBALL

  if [ ! -e "$FASTBIT_TARBALL" ]
  then
    wget $FASTBIT_URL || exit 1
  fi

  if [ ! -e "$FASTBIT_DIR" ]
  then
    tar zxovf $FASTBIT_TARBALL || exit 1
  fi
fi


if [ ! -e "lib" ]
then 
  if [ ! -e "$FASTBIT_DIR/configure" ]
  then
    echo "Fastbit download/extract failed!"
    exit 1
  fi;

  cd $FASTBIT_DIR

  if [ ! -e "Makefile" ]
  then
    ./configure --prefix=$CURDIR || exit 1
  fi

  make -j `cat /proc/cpuinfo|grep bogomips | wc -l` || exit 1

  make install || exit 1

fi
