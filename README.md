# Serial Console (sc)

A minimal terminal program allowing to use one machine to access the serial console of another machine.

# Building

If your system is POSIX compliant (or reasonably close to being so), just
running make will generate a binary.

The Makefile has a number of knobs to adjust the compiled in defaults.

Some systems don't have a working implementation of poll(2), among them
Mac OS X 10.4.  You can enable a workaround in the Makefile by adding
`-DHAS_BROKEN_POLL` to the `CFLAGS`.


# Changes

1.0
- Remove deprecated bcopy() and usleep(). (Rosen Penev)

0.96
- add "-k" and "-K" parameters which send keys/bytes every second.
  This is useful for serial consoles to enter the BIOS and you don't want to
  press the keys yourself continuously. Use the escape action 'k' to stop
  sending the keys/bytes.

0.95
- add "-d" parameters which sets a delay after writing a newline character.
- allow arbitrary characters to be composed with the escape character.

0.94
- Fix DTR setting code
- By default, do not use work-around for broken poll(2)

0.93
- Allow control characters specified as -e "^a" for the escape character.
- Make DTR control optional, add Linux compatible ioctl to set/reset DTR.

0.92
- First Public Release
