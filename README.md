# FolderDrop

Bookmark files in your folders to Raindrop.io.

## What is Folderdrop

Folderdrop is a command line tool that scans directories and create bookmarks in [Raindrop.io](https://raindrop.io/). You can:
- Filter files using regexes (for filtering by extensions for example)
- Use files from multiple folders
- Apply tags to the created bookmarks

## But why would you do that?

Currently I'm hosting a http file server that have tons of files. Unfortunatelly, it does not support tags and have a poor file searching, so I created this tool to make Raindrop my index.
Maybe in future I could expand it to allow scanning files in Google Drive, or other services so I could search files in multiple places through Raindrop.

## Getting started

Download one of the files from Github releases or build it on your machine.

Create a api key in raindrop by following their [documentation](https://developer.raindrop.io/v1/authentication/token). The Test Token is enough. You'll need to export it via environment variable to folderdrop.

```shell
export RD_TOKEN='<redacted>'
```

Now prepare a mount and execute folderdrop. A mount is a combination of path, collection and patterns, besides other configurations. Example to upload .txt files to a collection named my-uploads.

```shell
folderdrop -m 'mount' --path ./my-uploads --collection my-uploads --patterns '.*\.txt' --tags '#from:folderdrop' --link-prefix 'https://myhomeserver.net/api/files/'
```

If everything works, it is a good idea to use a configuration file instead of command line args (remember that for any overlapping options the command line ones will have precedence).
A configuration file the executes the same mounts:

```ini
[mount]
path=./my-uploads
collection=my-uploads
patterns=.*\.txt
tags=#from:folderdrop
link_prefix=https://myhomeserver.net/api/files/
```

Then simply run `folderdrop` with a file named `config.bs` in the same directory.

Multiple mounts may be defined with square brackets (in the configuration file) or `-m` or `--mount` (via command line arguments). Any options specified after them will be applied only to that mount.

```shell
folderdrop -m mount1 -p dir-1 ... \
  -m mount2 -p dir-1 ...
```

## Build

This project uses only two depencencies, [nlohmann/json](https://github.com/nlohmann/json) and [libcpr](https://github.com/libcpr/cpr).

IIRC when building on Ubuntu, you'll need to install the following packages:

```
apt install libssl-dev nlohmann-json3-dev libcurl4-openssl-dev
```

If coming from a clean install (with no development tools) install also:

```
apt install pkg-config g++ cmake
```

Execute the following commands:

```shell
mkdir build
cmake . -B build
cmake --build build
```
