# Pulse Oximeter

*This is a work in progress*

## What works
You can use the script build_cli.sh to build a command line tool that can read out your CMS50F pulse oximeter. This device appears under serveral names, mine is called Pulox PO 400.

The cli program uses a library that is written in ANSI-C for maximum compatibility. Use it everywhere where you can connect your device and have a POSIX layer.

## What will come
- A macOS app that can visualize and archive the recorded data.
- a CSV export that will resemble the original softwares CSV export for compatibility with whatever your physician uses.