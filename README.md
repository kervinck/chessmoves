Chessmoves
==========
Chess move and position (SAN/FEN) generation in C for use in scripting applications

`Chessmoves' is a small kit to make chess tree traversal available in a
scripting environment, such as Python or sh.

In the core it is a set of C functions for chess move making, wrapped for
easy command line use. The move generator is a simple and very reliable
mailbox-based set of functions originating from the MSCP v1.4 chess program.
For reference, MSCP has played 2.5 million games against human opponents
on the Internet Chess Servers FICS and ICC. The code has been heavily
cleaned up and it passes all known so-called 'perft' tests.

All input and output is in the form of FEN strings for positions.
SAN notation for moves is made available as well.

The output is comma-separated by default and designed to be easy to
process by other text processing programs.

TODO: give example
