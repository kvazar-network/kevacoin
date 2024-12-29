Berkley DB 4.8 build guide
==========================

Kevacoin should work with modern Berkley DB but if your distributive requires legacy version,
run following installation script (or watch the implementation details):

``` bash
./contrib/install_db4.sh `pwd`
export BDB_PREFIX='/given/path/to/db4'
./autogen.sh
./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include"
make
```