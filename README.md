# KevaCoin by Community

This project was created in 2021 to independently maintain the abandoned [kevacoin-core](https://github.com/kevacoin-project/kevacoin) ecosystem, at this moment includes following changes:

* Recent updates to libboost
* Additional features for the Qt client
    * Enhanced theme: dark style and monospace font (useful for tables and ASCII art in values)
    * Extended entries browser, which includes a transaction details tab
    * Extended entries editor with counter and key/value on-type validation
    * UI fixes: IPv6 proxy interface support, timing corrections, and more
* [Yggdrasil](https://yggdrasil-network.github.io) and [Mycelium](https://github.com/threefoldtech/mycelium) mesh networks support - mixed or single net mode (see `-onlynet`) to enhance your privacy and connectivity
* Additional seeds
* Flatpak bundle out of the box (see [releases page](https://github.com/kvazar-network/kevacoin/releases) or [build documentation](https://github.com/kvazar-network/kevacoin?tab=readme-ov-file#build))
* It's 100% compatible with the main Kevacoin Network

Instead of `master` use `kvazar` branch as main.
The `master` branch required for legacy [contributions](https://github.com/kevacoin-project/kevacoin/pulls) and get updates from the original upstream only.

Join the development by sending [PR](https://github.com/kevacoin-project/kevacoin/pulls) or just open [Issue](https://github.com/kevacoin-project/kevacoin/issues) for any questions!

## What is Kevacoin?

Kevacoin is a decentralized open source key-value data store based on Litecoin (which is in turn based on Bitcoin) cryptocurrency. Kevacoin is largely influenced by Namecoin [https://namecoin.org](https://namecoin.org), even though it serves very different purposes and works very differently.

## What does it do?

* Securely record keys and their values. Size of value is up to `3072` bytes. No hard limits on the number of keys.
* Update or delete the keys and their values.
* Maintain network-unqiue namespaces. Keys are grouped under namespaces to avoid name conflicts.
* Transact the digital currency kevacoins (KVA).

## What can it be used for?

As a decentralized key-value database, it can be used to store data for all kinds of applications, such as social media, microblogging, public identity information, notary service. Kevacoin has limited support for smart contracts (similar to Bitcoin and Litecoin), but one can still develop decentralized apps (dApps) on Kevacoin. The data is decentralized while the application logic is developed off the blockchain.

Our major observation for decentralized apps is that data is significantly more important than the application. In fact, that is the case for all kinds of applications. It is common these days to hear that companies rewrite their applications using better technologies, but it is rare for any of them to make big changes to their valuable data.

Take a look at [awesome-kevacoin](https://github.com/kvazar-network/awesome-kevacoin) catalog, also you're welcome to share new service there!

## Install

Please, visit the [Releases](https://github.com/kvazar-network/kevacoin/releases) page to get the latest stable version and precompiled binaries!

## Build

* [Flatpak](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/flatpak.md)
* [Fedora](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/build-fedora.md)
* [NetBSD](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/build-netbsd.md)
* [OpenBSD](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/build-openbsd.md)
* [OSX](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/build-osx.md)
* [Unix](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/build-unix.md)
* [Windows](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/build-windows.md)

## Connect

If you have any problems with the connection out of the box or any issues with your provider,
try alternative [DNS](https://github.com/kvazar-network/awesome-kevacoin#dns) and [Peers](https://github.com/kvazar-network/awesome-kevacoin#peers).

## License

Kevacoin Core is released under the terms of the MIT license. See [COPYING](COPYING) for more
information or see https://opensource.org/licenses/MIT.