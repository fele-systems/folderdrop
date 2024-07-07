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

## Build

This project uses only two depencencies, [nlohmann/json](https://github.com/nlohmann/json) and [libcpr](https://github.com/libcpr/cpr).

IIRC when building on Ubuntu, you'll need to install the following packages:

```
apt install libssl-dev nlohmann-json3-dev libcurl4-openssl-dev
```

Execute the following commands:

```shell
mkdir build
cmake -S build
cmake --build build
```
