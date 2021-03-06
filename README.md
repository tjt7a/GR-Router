GR-Router
=========

GNU Radio Router (GR-Router) module.

This is a module for GNU Radio used for balancing Software Defined Radio applications across multiple machines. GR-Router is composed of the following blocks:

Queue_Sink: This block accepts a stream of data and segments it, and then pushes the segments into a Queue. These segments need to be independently calculable.

Queue_Source: This block pops segments off of a segment Queue, and streams the data out.

Throughput: This block is meant to be placed in series with other GNU Radio blocks and prints out the data flow's throughput between the two blocks.

Throughput_Sink: This sink block can be connected to a second output of a block, and prints out the data flow's throughput.

Root Router: This Router block works to equally balance computable segments among its children.

Child Router: This Router block accepts computatable segments from its Parent and computes the segments. It then replies to it's parent with the result and its weight (for balancing).
