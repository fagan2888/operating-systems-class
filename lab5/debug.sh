#!/bin/bash
fusermount -u fp-mt
make pa5-endfs
sleep 1
./pa5-endfs mypass ~/Documents -d fp-mt