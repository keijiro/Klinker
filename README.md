Klinker
=======

**Klinker** is a Unity package for sending/receiving video streams via
BlackMagic Desktop Video hardware like DeckLink, Intensity, etc.

Klinker is still under development. It has several known/unknown issues. It
might be sufficient for testing or proof-of-concept projects, but please
consider using other commercial plugins for serious applications.

System Requirements
-------------------

- Unity 2019.4 or later
- Windows system
- BlackMagic Desktop Video software
- Desktop Video compatible hardware (DeckLink, Intensity, etc.)

How To Install
--------------

This package uses the [scoped registry] feature to import dependent packages.
Please add the following sections to the package manifest file
(`Packages/manifest.json`).

To the `scopedRegistries` section:

```
{
  "name": "Keijiro",
  "url": "https://registry.npmjs.com",
  "scopes": [ "jp.keijiro" ]
}
```

To the `dependencies` section:

```
"jp.keijiro.klinker": "0.1.0"
```

After changes, the manifest file should look like below:

```
{
  "scopedRegistries": [
    {
      "name": "Keijiro",
      "url": "https://registry.npmjs.com",
      "scopes": [ "jp.keijiro" ]
    }
  ],
  "dependencies": {
    "jp.keijiro.klinker": "0.1.0",
...
```

[scoped registry]: https://docs.unity3d.com/Manual/upm-scoped.html
