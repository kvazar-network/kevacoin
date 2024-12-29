Fedora build guide
======================
(tested on Fedora 41)

* `sudo dnf install git autoconf automake libtool make boost-devel protobuf-devel qt5-qtbase-devel qt5-linguist libdb-cxx-devel`
* `git clone https://github.com/kvazar-network/kevacoin.git`
* `cd kevacoin`
* `./configure --with-incompatible-bdb --with-gui`
* `make`
* run `src/qt/kevacoin-qt`
