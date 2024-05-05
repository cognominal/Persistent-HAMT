NAME
====

Persistent::HAMT - MOARVM based persistent hashes and stacks

SYNOPSIS
========

At this stage this is only a scaffolding.

```raku
use Persistent::HAMT;
my @a is repr('HAMTArray);
my %a is repr('HAMTHash);
```

DESCRIPTION
===========

This is the user facing future documentation


Persistent::HAMT provides 2 representations, one for stacks, one for hashes.
They exploit persistent properties of HAMTs to share storage for different 
data structures evolved from the same one.

They can be used with existing language support, but for more functionnal style,
specific language support will be needed.

  my @a is repr('HAMTArray);
  # various operation that mutate @a
  my @b repr('HAMTArray) = @a;
  # various operation that mutate @b

There are stacks so array operations which are not stack like (inserts, shifts) will fail.

Same thing for hashes but the repr is different.

  my %a is repr('HAMTHash);


Start [HAMT](https://en.wikipedia.org/wiki/Hash_tree_(persistent_data_structure)) for learning
the properties of peristent tries.
This lib is just an adaptation of [mkirchner/hamt](https://github.com/mkirchner/hamt) to be used by
MoarVM reprs.

Scaffolded using [App::Mi6](https://github.com/skaji/mi6)

AUTHORS
======

* Stéphane Payrard <cognominal@gmail.com>
* Marc Kirchner
* from ideas by Phil Bagwell


COPYRIGHT AND LICENSE
=====================

Copyright 2024 Stéphane Payrard

This library is free software; you can redistribute it and/or modify it under the Artistic License 2.0.

