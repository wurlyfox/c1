Copy each of the asset files (.NSD/.NSF) on the Crash Bandicoot game disc from its respective `/S*` directory into this directory. Then rename each file from the uppercase format `S00000%%.NS[F,D]` format to the lowercase format `s00000%%.ns[f,d]`.

If you instead have a disc image (`.bin` or `.iso`) made from a copy of the game you own, run the following from the main project directory to extract and rename the files into this directory:

```
python tools/extract_streams.py crash.bin
```

