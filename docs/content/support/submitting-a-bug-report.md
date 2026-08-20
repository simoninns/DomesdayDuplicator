# Submitting a bug report

If you find a bug in the capture application please use the Github issue reporting tool available from [this link](https://github.com/simoninns/DomesdayDuplicator/issues).

## What to include

A log is worth more than a description, and the application will write one for you. Start it
from a terminal with:

```bash
ddd-gui --log-level debug --log-file ddd.log
```

then make the fault happen and attach `ddd.log` to the report. It opens with the build and
the platform it ran on, and carries the application's own account of what it did — a
capture's throughput and buffer levels, or every step of a firmware update or a board
bring-up, whichever went wrong. [Command line](../capture-gui/command-line.md) describes
both switches and what each level records.

Without a log, the two things always worth quoting are the commit from **Help ▸ About** and
the contents of the Log panel (**View ▸ Panels ▸ Log**) — select the records there and
press **Ctrl+C**, or right-click ▸ **Copy**, to paste them into the report.

If you'd like to request new features please use the Github issue reporting tool and describe clearly your requirements and, if possible, the justification for inclusion.

If you'd like to support the development of this application you are welcome to contribute code via Github, or simply make a [donation](donations.md).