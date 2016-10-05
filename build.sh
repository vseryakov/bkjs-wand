export BKJS_PREFIX=$(pwd)/build
export BKJS_DEPS=$BKJS_PREFIX/deps
export CFLAGS="$CFLAGS -fPIC -g -I$BKJS_PREFIX/include"
export CXXFLAGS="$CXXFLAGS -fPIC -g -I$BKJS_PREFIX/include"
export LDFLAGS="$LDFLAGS -L$BKJS_PREFIX/lib"
export PKG_CONFIG_PATH=$BKJS_PREFIX/lib/pkgconfig:$PKG_CONFIG_PATH
[ -d /opt/local/lib ] && export LDFLAGS="$LDFLAGS -L/opt/local/lib" && export CFLAGS="$CFLAGS -I/opt/local/include" && export PKG_CONFIG_PATH=/opt/local/lib/pkgconfig:$PKG_CONFIG_PATH
[ -d /usr/local/lib ] && export LDFLAGS="$LDFLAGS -L/usr/local/lib" && export CFLAGS="$CFLAGS -I/usr/local/include" && export PKG_CONFIG_PATH=/usr/local/lib/pkgconfig:$PKG_CONFIG_PATH
mkdir -p $BKJS_DEPS

pkg-config --silence-errors --exists libopenjp2
if [ ! -d $BKJS_DEPS/openjpeg ]; then
   #(cd $BKJS_DEPS && svn -r r2871 checkout http://openjpeg.googlecode.com/svn/trunk/ openjpeg)
   (cd $BKJS_DEPS && git clone https://github.com/uclouvain/openjpeg.git)
fi
(cd $BKJS_DEPS/openjpeg && cmake -DCMAKE_C_FLAGS="$CFLAGS" -DCMAKE_INSTALL_PREFIX=$BKJS_PREFIX -DBUILD_SHARED_LIBS:bool=off -DBUILD_CODEC:bool=off . && make)
(cd $BKJS_DEPS/openjpeg && make install)

pkg-config --silence-errors --exists Wand
if [ ! -d $BKJS_DEPS/ImageMagick ]; then
   mkdir -p $BKJS_DEPS/ImageMagick
   curl -OL http://www.imagemagick.org/download/ImageMagick.tar.gz
   tar -C $BKJS_DEPS/ImageMagick --strip-components=1 -xzf ImageMagick.tar.gz && rm -rf ImageMagick.tar.gz
fi
(cd $BKJS_DEPS/ImageMagick && [ ! -f Makefile ] && ./configure --prefix=$BKJS_PREFIX \
                 --disable-docs \
                 --disable-installed \
                 --disable-shared \
                 --disable-deprecated \
                 --enable-zero-configuration \
                 --without-x \
                 --without-magick-plus-plus \
                 --without-perl)
(cd $BKJS_DEPS/ImageMagick && make install)
exit 0
