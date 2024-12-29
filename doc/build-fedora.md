Fedora build guide
======================
(tested on Fedora 41)

## Dependencies

``` bash
sudo dnf install git autoconf automake libtool make \
         boost-devel protobuf-devel qt5-qtbase-devel qt5-linguist libdb-cxx-devel
```

## GUI

* `git clone https://github.com/kvazar-network/kevacoin.git`
* `cd kevacoin`
* `./configure --with-incompatible-bdb --with-gui`
  * to build with legacy BerkleyDB version, visit [this guide](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/berkleydb-48.md)
* `make`
* run `src/qt/kevacoin-qt`
