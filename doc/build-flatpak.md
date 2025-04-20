# The Flatpak bundle

> [!TIP]
> Check out latest [Releases](https://github.com/kvazar-network/kevacoin/releases) to get the precompiled bundle!

## Build

> [!NOTE]
> * the `libdb` module (a.k.a. Berkeley DB) requires the `CFLAGS=-Wno-error=implicit-function-declaration` to disable warnings in the recent runtime version `24.08`. This option is not yet necessary for `23.08`, but that version is not in use as deprecated;
> * at this moment, the newer versions of `boost` and `miniupnpc` than defined in the manifest are not compatible and require an update to the `kevacoin-core` API (see [dependencies](https://github.com/kvazar-network/kevacoin/blob/kvazar/doc/dependencies.md) for details)

### Get source

``` bash
git clone https://github.com/kvazar-network/kevacoin.git && cd kevacoin
```

### Compile and install

``` bash
flatpak-builder --force-clean build\
                --install-deps-from=flathub\
                --install\
                --repo=repo\
                --user\
                io.github.kvazar_network.kevacoin-qt.json
```

### Launch

Find and launch `KevaCoin` from the application menu or use the following command to launch with CLI:

``` bash
flatpak run io.github.kvazar_network.kevacoin-qt
```

### Share the bundle

Follow these steps if you want to build the latest `.flatpak` file from the repository and share it with others:

* `cd kevacoin` - navigate to the project source directory
* `git pull` - make sure the source is up to date
* `flatpak build-bundle repo kevacoin-qt.flatpak io.github.kvazar_network.kevacoin-qt` - create the bundle (`kevacoin-qt.flatpak`)
* `flatpak install --user kevacoin-qt.flatpak` - install from the `kevacoin-qt.flatpak` bundle
    * to uninstall the bundle, run: `flatpak uninstall io.github.kvazar_network.kevacoin-qt`

### Feedback

Please, create new [Issue](https://github.com/kvazar-network/kevacoin/issues) if you have any questions or feel free to contribute!