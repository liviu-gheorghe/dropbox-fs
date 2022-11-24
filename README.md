# Dropbox FS

This project aims to implement a minimal Linux FUSE filesystem providing access to Dropbox data for a specific account.  It exposes a simple CLI API for managing Dropbox assets, including operations like copying / moving / deleting files or creating new directories.

The project uses [dbxcli](https://github.com/dropbox/dbxcli) internally, a command line tool for Dropbox users and team admins. 

---

TODO:

    - Prerequisites / System Requirements
    - Installation
    - Usage