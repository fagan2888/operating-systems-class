#!/bin/bash
fusermount -u fp-mt
make pa5-endfs
./pa5-endfs mypass ~/Dropbox fp-mt