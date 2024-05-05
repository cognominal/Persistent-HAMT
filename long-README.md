## blurb

We discuss here how to use hamt to implement a tree sitter for raku.
That is, to adapt hamt to raku (with MoarVM as a backend) GC.

`hamt` denotes the original [implementation](https://github.com/mkirchner/hamt)
intended to support immutable hashes. It can be trivially adapted support
immutable stacks as well (as opposed to arrays which implies insertion).
`hamt-raku` denotes the present adaptation.

The adaptations of raku and vscode to exploit  data from the persistent world provided
by hamt-raku will be treated/discussed elsewhere. 

## initial goal

At this point, we want to create reprs for hamt_node, hash and stacks that are
dynamically loaded. And also create a nqp op, also dynamically loaded, that
freeze mutable structures of the world. Also provide tests. Later, for performance sake, we
will probably use reference counting for hamt_nodes, so a repr for
them will not be necessary.

## Prerequisite to read this doc

Rakudo is the implementation of raku. Nqp is a simplified raku used to
implement raku. MoarVM is the preferred raku backend written in C. See below,
[hamt README](https://github.com/mkirchner/hamt/blob/main/README.md) and
[rakudo](https://github.com/rakudo/rakudo/tree/main/docs)/[nqp](https://github.com/Raku/nqp/tree/main/docs)/[MoarVM](https://github.com/MoarVM/MoarVM/tree/main/docs)
docs for definitions of the terms used in this initial blurb.

# The doc per se

[Treesitter](https://en.wikipedia.org/wiki/Tree-sitter_(parser_generator))
based syntaxic highlighting of raku and its slangs, possibly changing on the
fly, necessitates an approach different from existing ones. With the fluidity
of raku syntax due to slangs, using a specialized parser for that purpose, as
is now the norm, is a non starter. Instead we will use the existing parser but
use a shared persistent structure so to be able to restart parsing from any
point where the user edits code.

The whole point of treesitters is being responsive to provide timely syntactic
highlighting when editing code, so we must be careful of the impact of the
implementation.

So we want to use
[persistent](https://en.wikipedia.org/wiki/Persistent_data_structure)
[tries](https://en.wikipedia.org/wiki/Trie), that is,
[hamts](https://en.wikipedia.org/wiki/Hash_array_mapped_trie) to compactly
store the history of the
[world](https://github.com/Raku/nqp/blob/main/src/NQP/World.nqp).

The world is holding the state of raku/npq compilation. the World is
constituted of hashes and stacks so it can be implemented in term of hamt
tries. 

We want minimal impact on existing code, be it MoarVM or nqp, except may be in
World.pm. It implies creating new MoarVM representations to implement
persistent hashes and stacks to support a (not so) brave new world. We want to
avoid changing the structure of [6 model
representations](https://github.com/Raku/nqp/blob/main/docs/6model/overview.markdown#representations)
(reprs), that is avoiding adding new methods in
[MVMREPROps](https://github.com/MoarVM/MoarVM/blob/8c413c4f0ce0fed7c2accf92db496112c205a206/src/6model/6model.h#L561)
which instances acts as a
[vtable](https://en.wikipedia.org/wiki/Dispatch_table). For simplicity we don't
care about implementing concurrent persistent structures like is done in
[ctries](https://en.wikipedia.org/wiki/Ctrie).



## not changing reprs structure

Using only persistent structures has two drawbacks:

* Each operation which previously was a mutation, now returns a new
persistent structure. This implies adding new entries to MVMREPROps to support
methods that returns that value. Growing MVMREPROps would impact the memory
caches. 
* Creating a persistent structure instead of a mutation implies a lot
of path copying meaning using a lot of memory.

Instead we use mutable structures and at specific points of the compiled code,
we "freeze" a mutable structure into a mutable one using a new nqp opcode.

## GC 

The whole point of this adaptation is to support MoarVM GC. We dont want to
negatively impact GC. So we probably eventually use use reference counting for
`hamt_node`s so only the root of tries need to be marked by the mark phase of
GC.

## testing the tries without modifying nqp or MoarVM

Not changing the structure of MVREPROps means we can simply dynamically
register two new reprs to implement hashes and stacks using HAMTs.
Similarly we add dynamically a npq op to freeze these mutable structures. 

## reference counting

I differ the implementation of reference counting because I want my first
implementation to be a module without changing anything to MoarVM or nqp.
I just don't want to juggle with too many balls at a time.

With reference counting the GC would not mark stuff beyond the root of our structures.
As a result, values in these structures would be wrongly garbage collected.
So we need to mark them so the GC would know not to free them. That means modifying
the code of MoarVM.

## invariants

We denbote by table, `hamt_node.table`. That is the table part for the hamt_node union.

Frezing a mutable hamt structure means to create a immutable hamt structure meaning 
create the appropriate constant hamt_nodes with the appropriate path copying from the 
mutable hamt_nodes.

The part involving reference counting are irrelevant for the pure GC implementation.

A mutable trie root is a mutable table. A child of a mutable table can be
either mutable or immutable. A children of an immutable table is mutable. A
mutable table is not shared and is identified by a count field of 2^64 - 1 for
reference counting The root table of an immutable subtree has a `count` field
which value is the count of tries that share it.

Freeing a immutable trie decrement the count field of its tables.
If a table count field become 0, this table and its descendants are freed.

A immutable trie is made from a mutable one by duplicating the root table
converting all the mutable table to immutable one with a `count` field set to 1.

## World.pm, QAST and MAST

When updating World.nqp, code involving hashes and arrays are
translated in QAST::VarWithFallBack which are processed in still mysterious
way for me.

idea. Doubled sigil would mean hamt hash/array. 
Code of the form

  %%b := %%a{'foo'} := 'bar';

would be short for  
  
  %%a{'foo'} := 'bar';   # update the hamt mutable %%a
  %%b := %%a;            # freeze %%a into %%b

Unclear how to process that in QAST/MAST


## implementation details

now just a scaffolding of things to come.

I now start from a APP::Mi6 scaffolded raku repo where I copied the hamt implementation.
My hope of going with changes to MoarVM are dashed. I will need to modify the MVMIter repr.

Initial goal :  implement the hamt-node repr and test if from nqp.


Apparently [LibraryMake](https://raku.land/zef:jjmerelo/LibraryMake) is the
way to "simplify building native code for a Raku module".

[register_repr](https://github.com/MoarVM/MoarVM/blob/e149d3de5bf1514008401d0f63f040e03f057217/src/6model/reprs.c#181)
register a repr. Apparently there is no nqp op that wraps it.

MVM_register_extop. Example of 
[use](https://github.com/rakudo/rakudo//blob/969ae326d9bca4e4d5b0906f4d53e76bf702e020/src/vm/moar/ops/perl6_ops.c)


`loader.raku` will contain code to load the reprs and the freeze op

`reprs/` contains the repts
* `reprs/hamt-array.[hc]`
* `reprs/hamt-hash.[hc]`
* `reprs/hamt-node.[hc]`
`hamt` contains the hamt code independannt from MoarVM

The initial idead was that hamt-node.c will contain no method except for garbage collecting.
In fact it will have array indexing for testing.