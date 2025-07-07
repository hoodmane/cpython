#!/bin/sh
exec ~/.jsvu/bin/v8 --enable-os-system ./d8_entry.mjs -- "$@"
