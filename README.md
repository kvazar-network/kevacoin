# KevaCoin by Community

This project created to keep alive [Kevacoin Core](https://github.com/kevacoin-project/kevacoin) ecosystem.

Contains QT and Boost updates, minor cosmetic changes and 100% compatible with the Kevacoin Network.

Instead of `master` we're using `kvazar` branch here as main.
The `master` branch is only for legacy [contributions](https://github.com/kevacoin-project/kevacoin/pulls).

Join to development by sending [PR](https://github.com/kevacoin-project/kevacoin/pulls) or just open [Issue](https://github.com/kevacoin-project/kevacoin/issues) for any questions!

# What is Kevacoin?

Kevacoin is a decentralized open source key-value data store based on Litecoin (which is in turn based on Bitcoin) cryptocurrency. Kevacoin is largely influenced by Namecoin [https://namecoin.org](https://namecoin.org), even though it serves very different purposes and works very differently.

# What does it do?

* Securely record keys and their values. Size of value is up to `3072` bytes. No hard limits on the number of keys.
* Update or delete the keys and their values.
* Maintain network-unqiue namespaces. Keys are grouped under namespaces to avoid name conflicts.
* Transact the digital currency kevacoins (KVA).

# What can it be used for?

As a decentralized key-value database, it can be used to store data for all kinds of applications, such as social media, microblogging, public identity information, notary service. Kevacoin has limited support for smart contracts (similar to Bitcoin and Litecoin), but one can still develop decentralized apps (dApps) on Kevacoin. The data is decentralized while the application logic is developed off the blockchain.

Our major observation for decentralized apps is that data is significantly more important than the application. In fact, that is the case for all kinds of applications. It is common these days to hear that companies rewrite their applications using better technologies, but it is rare for any of them to make big changes to their valuable data.

Visit [awesome-kevacoin](https://github.com/kvazar-network/awesome-kevacoin) to learn more!

## Build

### Linux

```
git clone https://github.com/kvazar-network/kevacoin.git
cd kevacoin`
./contrib/install_db4.sh `pwd`
export BDB_PREFIX='/given/path/to/db4'
./autogen.sh
./configure BDB_LIBS="-L${BDB_PREFIX}/lib -ldb_cxx-4.8" BDB_CFLAGS="-I${BDB_PREFIX}/include"
make
```

## License

Kevacoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.