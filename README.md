==============================================================================
Toad - A collection of tools to generate datafiles for Frog
==============================================================================

    by Ko van der Sloot & Antal van den Bosch
    Centre for Language and Speech Technology, Radboud University
    Licenced under the GNU Public Licence v3

This first release contains the main Frog datagenerator **froggen**

froggen enables you to create datafiles for tagging and lemmatizing with ``frog``
in any language and tag set.

other contained tools, (experts only) are:

* ``morgen` - 	generate morphological (MBMA) datasets based on CELEX data
* ``makembma``	- old preliminary version of morgen
* ``makemblem``	- old preliminary version of the lemmatizing part of ``froggen``
* ``checkmbma``	- check an MBMA configuration for inconsistencies
* ``checkmblem`` - check a lemmatizer configuaration for inconsistencies
* ``testmbma`` - another MBMA tester

The last three programs need additional data which are NOT provided in this
package. 

This due to size and/or rights.

--------------------------------
Installation Instructions
--------------------------------

To install toad, first consult whether your distribution's package manager
has an up-to-date package.  If not, for easy installation of toad, it is
included as part of our software distribution LaMachine:
https://proycon.github.io/LaMachine .

To compile from source instead:
* ``sh bootstrap.sh``
* ``./configure``
* ``make``
* ``make install``
* *optional:* ``make check``

--------------------------------
Documentation
--------------------------------

For ``froggen``, consult the man page and/or section 4.4 in the Frog manual.

Also see ``README.mblem`` for ``makemblem`` and ``README.mbma`` for
`'makembma``.


