# nymea-update-plugin-rauc

The `nymea-update-plugin-rauc` provides a platform update controller that plugs the nymea update framework with RAUC based update systems.

The update plugin makes use of the RAUC DBus API and provides an easy way to perform system updates from different sources, including self hosted image servers.


## Building

The project uses qmake:

```
mkdir -p build
cd build
qmake ..
make -j$(nproc)
```

## RAUC D-Bus interface generation

The RAUC D-Bus proxy (`raucinstallerinterface.h/.cpp`) is generated from an
introspection XML file (`rauc-introspection.xml`). To refresh it, first extract
the XML from a target that runs RAUC, then re-run the Qt D-Bus XML compiler.

### 1) Generate the introspection XML on the target

Run one of the following on the target (system bus):

```
busctl introspect --xml --system de.pengutronix.rauc / > rauc.xml
```

or:

```
gdbus introspect --system --dest de.pengutronix.rauc --object-path / --xml > rauc.xml
```

or:

```
qdbus --system de.pengutronix.rauc / org.freedesktop.DBus.Introspectable.Introspect > rauc.xml
```

Copy `rauc.xml` into the repo as `rauc-introspection.xml`.

### 2) Generate the proxy with qdbusxml2cpp

From the repo root:

```
qdbusxml2cpp -p raucinstallerinterface -c RaucInstallerInterface -i raucdbustypes.h rauc-introspection.xml de.pengutronix.rauc.Installer
```

This updates:
- `raucinstallerinterface.h`
- `raucinstallerinterface.cpp`

`raucdbustypes.h` contains minimal typedefs expected by the generated code.

### Keeping it up to date

- Re-run the steps above whenever RAUC is upgraded or the D-Bus API changes.
- Commit the updated `rauc-introspection.xml` together with regenerated proxy
  files so the build stays reproducible.

## Repository configuration

The default repository location can be placed in `/etc/nymea/rauc-update.conf`. This is the default location the update manager will search for, unless there is an update file located in `/var/lib/nymea/rauc-update.conf` where custom modifications can be done.

Currently only the repository type `selfhosted` is supported and as authentication type `basic` for basic authentication or `none` (default). See next chapter for more information on how set up a self-hosted RAUC image repository being compatible with this update plugin.

```json
{
    "systemName": "My awesome platform",
    "systemDescription": "Platform doing exactly what it is meant for.",
    "systemVendor": "company name",
    "repositories": [
        {
            "name": "Stable releases",
            "description": "In this repository you can find the stable releases for your platform.",
            "url": "https://rauc-update.example.com/stable",
            "repositoryType": "selfhosted",
            "authenticationType": "basic",
            "userName": "username",
            "password": "secret-password"
        },
        {
            "name": "Beta releases",
            "description": "In this repository you can find the next release candidates which are not stable yet.",
            "url": "https://rauc-update.example.com/beta",
            "repositoryType": "selfhosted",
            "authenticationType": "basic",
            "userName": "username",
            "password": "secret-password"
        },
        {
            "name": "Alpha releases",
            "description": "This repository can be used to test the latest development. Anything can happen, everything can change.",
            "url": "https://rauc-update.example.com/alpha",
            "repositoryType": "selfhosted",
            "authenticationType": "basic",
            "userName": "username",
            "password": "secret-password"
        }
    ]
}
```

## Self hosted update server

In order to make the hosting of your own update images as easy as possible, a simple tool for hosting your own update server is available.

Host a folder where the update information can be found on your server. A webserver like apache or nginx can provide restricted access and handle all the SSL configurations. 

Here an example how to host your own image server. In this example the webserver is hosting the `/var/www/rauc-updates` folder and the domain rauc-update.example.com is pointing to this folder.

Assuming your CI generates as output an image file you want to distribute, the shipped [nymea-rauc-update-tool](/nymea-rauc-update-tool) can be used to manage the updates.

Multiple channels like alpha, beta, stable, or any other channel are possible.


```bash
nymea-rauc-update-tool -i my-rootfs-release-1.0.0.image -o /var/www/rauc-updates/stable
```

This will generate a `/var/www/rauc-updates/stable/release.json` file looking like this:

```json
{
    "bundle_md5sum": "d26b9f2bd8d7a7cb5d7d888c94696983",
    "changelog": "Changes in this version:\n - Add cool feature A\n - Fix bug xyz",
    "compatible": "System XY",
    "image": "my-rootfs-release-1.0.0.image",
    "release_timestamp": 1769424436,
    "rootfs_sha256": "3639f9f791a62c3c38b906264a0bd8581abbdfb6799b75764fd9fa86ccc83973",
    "version": "1.0.0"
}
```

The properties `version`, `compatible` and the `rootfs_sha256` will be extracted from the given image file. An update will be available, 
if the current rootfs SHA256 of the booted image does not match the current selected repository `rootfs_sha256`. The version is considered only informative for the user.

## Handle automatic reboots

Since nymea could be shut down during the system update for configuration sync, RAUC should handle an the automatic reboot on successfull installation. The best approach to do so is using the post install handler.

The `nymea-update-plugin-rauc` will create the `/run/firmware-update-reboot-requested`, with following handler in `/usr/lib/rauc/post-install.sh` the system will reboot automatically once the installation process has finished.


```bash
#!/bin/sh

# Trigger a reboot after the bundle finished installing, if the file
# /run/firmware-update-reboot-requested exists, otherwise do nothing.
#
# If the update has been performed by a service which might be already stopped
# due to configuration sync, the reboot can be automatically triggered once the
# installation process has finished successfully.

REBOOT_REQUESTED_FILE=/run/firmware-update-reboot-requested
if [ ! -f "$REBOOT_REQUESTED_FILE" ]; then
    exit 0
fi

logger -t rauc-post-install "Reboot has been requested using $REBOOT_REQUESTED_FILE file"
sync

# Prefer systemd reboot if available
if command -v systemctl >/dev/null 2>&1; then
        systemctl reboot
else
        reboot
fi

exit 0

```

License
-------

This project is licensed under the GPL-3.0-or-later. See the source headers for details or `/usr/share/common-licenses/GPL-3` on Debian systems. 
