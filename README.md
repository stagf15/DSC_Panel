# DSC_Panel
Libraries and example sketches to allow Arduino to act as a virtual Keypad for a DSC Powerseries 18XX alarm panel

I've just merged a major change, mainly to DSC.cpp.  Instead of recording an entire word as a binary string (which could use up to 80 or 100 bytes for just the initial word, then triple that for the old and new word strings, and then double all of that for the panel and keypad), it now stores the word as a byte array, mathematically summing the bits as they are received.  This saves an incredible amount of memory, now using less than 100 bytes for all of the data which would previously be stored using strings and up to 600 bytes.  Now an arduino uno will run this while also using the Ethernet library.  This combination used to cause a memory crash after a few seconds.

Other things that have changed are the way the global variables are structured.  All globals (for the most part) used to be contined in one big structure called dscGlobal.  Now the globals are broken down into three different stuctures called keybus, panel and keypad.  This drives a few changes in the example sketches, so be sure to look at those.  Mainly, instead of accessing the global variables directly, I've created functions to get needed variables.  Eventually, those variables will be made private, to only be accessed through the calling functions, but this hasn't been implemented yet.  

Also, a few of the names changed for the various panel and keypad formatting functions based on what they did.  The examples should clearly show what has changed.

I will now be working with "rogueturnip" on adding write capability to this library.  We think we have a plan and a way forward.  Stay tuned (and sorry if the updates are sparse... turns out life is busy!).
