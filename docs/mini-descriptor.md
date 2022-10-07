# MiniDescriptor Specification

<!-- DO NOT SUBMIT without changing the go-link -->

go/upb-mini-descriptor

<!--*
# Document freshness: For more information, see go/fresh-source.
freshness: { owner: 'haberman' reviewed: '2022-09-05' }
*-->

This is a spec for the upb MiniDescriptor format. MiniDescriptors are a very
small serialization format for storing and transmitting schema data for
protobufs.

MiniDescriptors are much smaller than full descriptors. By omitting names and
most options, they weigh in at roughly 1/60th the size of regular descriptors.
MiniDescriptors contain only the data that is necessary to parse and serialize
messages in protobuf binary format.

The format documented here is specific to upb. Other projects have
MiniDescriptor formats that are similar, but not exactly identical, to this one.

## Acknowledgements

This document and spec are extensions of previous work done by Kevin O'Connor,
whose own work extended that of Rafi Kamal.

## Current Status

Currently MiniDescriptors are in a pre-release state. The code is implemented
and is being used in experiments, however the format is still subject to change.

Once there is an official design review and final approval, the format will be
considered "released" and the compatibility guarantees documented below will go
into effect. Until that time, there are no compatibility guarantees.

## Compatibility Guarantees

MiniDescriptors are designed to be embedded in generated code, which may be
shipped over the wire and loaded by a different version of the upb runtime.

This means that MiniDescriptors must provide stability and compatibility over
time.

Once MiniDescriptors are officially approved for release to production, the
following compatibility guarantees will apply:

1.  **Non-Ambiguity**: A MiniDescriptor will have a stable meaning over time.
    Upgrading or downgrading the upb runtime will never change the meaning of a
    MiniDescriptor. It is possible that some versions of the runtime may *fail
    to load* a MiniDescriptor that is too old or too new, but updating the
    runtime should never run the risk of corrupting your data.
2.  **Clear Rejection**: If a given MiniDescriptor cannot be loaded by the upb
    runtime because the format is too old or too new, the runtime will
    automatically detect this and provide a clear error status.
3.  **Compatibility Windows**: TODO: talk with customers to understand their
    compatibility window requirements better.

## Specification

### Base92

The majority of the data provided by the MiniDescriptor will be encoded as a
string using a modified Base92 implementation. That is, we will be using the
ASCII (dec) range of [32, 126], but omitting 34 ", 39 ', and 92 \ as they can
require additional string escaping. This range will directly map to values [0,
91]. There is no continuation/padding, so all encoded values must be within that
range.

Since the intention for the MiniDescriptors are to be printed literally in
strings via code-generation, we choose this range for several reasons:

* We skip [0, 31] to avoid the non-printable control characters
* We skip ", ', and \ to avoid needing to emit escapes
* This is the ASCII character range that can be represented using a single byte

If more room is required in the future there are a few options: You can start
using ", ', and \\, however, since we currently skip around them you’ll need to
assign them special values at the end of the range. That is, their decoded value
will not align with their position in the ASCII range. Additionally, care will
need to be taken to ensure that they are properly escaped, costing two bytes for
each of these that needs to be escaped. You can expand further into UTF-8, but
you’ll start paying higher costs per character for those.

Why not UTF-16? Since all JavaScript strings are represented as UTF-16 in memory
we don’t get the same one-byte-per-character benefit there. However, we do get
the benefit on the wire. In theory we could use UTF-16 and get higher wire cost,
but in turn get more data density in memory. This isn’t particularly helpful to
this application though. The feature that would most benefit from this is field
skips, but those already tend to be smaller. Large field skips are relatively
rare. Therefore, the smaller wire cost is likely going to outweigh the benefits
in memory.

## Encodings

### Version Tag

(used by: all MiniDescriptor types)

Every encoded MiniDescriptor begins with a one-character version tag,
of which the following are currently defined:

ch | meaning
:- | :------
!  | Enum (V1)
\# | Extension (V1)
%  | Map (V1)
$  | Message (V1)
&  | Message Set (V1)

#### Grammar

<version_tag> ::= <enum_tag> | <extension_tag> | <map_tag> | <message_tag> | <message_set_tag> \
<enum_tag> ::= <enum_tag_v1> \
<enum_tag_v1> ::= '!' \
<extension_tag> ::= <extension_tag_v1> \
<extension_tag_v1> ::= '#' \
<map_tag> ::= <map_tag_v1> \
<map_tag_v1> ::= '%' \
<message_tag> ::= <message_tag_v1> \
<message_tag_v1> ::= '$' \
<message_set_tag> ::= <message_set_tag_v1> \
<message_set_tag_v1> ::= '&' \

### Skips

(used by: Enums, Messages)

A gap in field numbers is represented with the skips range which is a 5-bit
segment variable integer in little-endian format. The “gap” is defined as
the difference between the next field number and the previous field number.
If a skip is needed at the start of the descriptor, it should be assumed the
previous field was 0.

For example, say we wanted to encode a skip of 32 field numbers. Since this
exceeds a single 5-bit segment, we’ll use two characters to represent the skip.
This would be 0x00 0x01, but we must add 60 to each value to put in the skip
field range, then convert it to the base92 characters. The result would be _
follwed by `.

char | skip  | char | skip
:--- | :---- | :--- | :---
_    | 0     | o    | 16
`    | 1     | p    | 17
a    | 2     | q    | 18
b    | 3     | r    | 19
c    | 4     | s    | 20
d    | 5     | t    | 21
e    | 6     | u    | 22
f    | 7     | v    | 23
g    | 8     | w    | 24
h    | 9     | x    | 25
i    | 10    | y    | 26
j    | 11    | z    | 27
k    | 12    | {    | 28
l    | 13    | \|   | 29
m    | 14    | }    | 30
n    | 15    | ~    | 31

#### Grammar

<skip_> ::= '_'..'~'

### Field Types

(used by: Extensions, Maps, Messages)

Field Type | Base 92 Value | Singular Char | Repeated Char
:--------- | :------------ | :------------ | :------------
Double     | 0             | ' ' (space)   | 6
Float      | 1             | !             | 7
Fixed32    | 2             | #             | 8
Fixed64    | 3             | $             | 9
SFixed32   | 4             | %             | :
SFixed64   | 5             | &             | ;
Int32      | 6             | (             | <
UInt32     | 7             | )             | =
SInt32     | 8             | *             | >
Int64      | 9             | +             | ?
UInt64     | 10            | ,             | @
SInt64     | 11            | -             | A
OpenEnum   | 12            | .             | B
Bool       | 13            | /             | C
Bytes      | 14            | 0             | D
String     | 15            | 1             | E
Group      | 16            | 2             | F
Message    | 17            | 3             | G
ClosedEnum | 18            | 4             | H
(unused)   | 19            | 5             | I

#### Grammar

<field_type> ::= <singular_field> | <repeated_field> \
<singular_field> ::= ' '..'5' \
<repeated_field> ::= '6'..'I'

### Field Modifiers

(used by: Extensions, Maps, Messages)

Any encoded field may be immediately followed by a field modifier which adds
semantic meaning to the field which precedes it. A field modifier is encoded
as set of low-endian bitfields:

bit | modifier
:-- | :-------
0   | Reverse Packed/Unpacked; indicates deviation from the default as indicated by the Message Modifiers. Only meaningful for repeated fields.
1   | Field is required
2   | Field is a proto3 singular

#### Grammar

<field_modifier> ::= 'L'..'['

### Enum Masks

(used by: Enums)

An enum mask is a 5-bit integer which denotes values that appear in an enum.

val | ch  | meaning    | val | ch  | meaning
:-- | :-- | :--------- | :-- | :-- | :---------
0   |     | mask 00000 | 16  | 2   | mask 10000
1   | !   | mask 00001 | 17  | 3   | mask 10001
2   | #   | mask 00010 | 18  | 4   | mask 10010
3   | $   | mask 00011 | 19  | 5   | mask 10011
4   | %   | mask 00100 | 20  | 6   | mask 10100
5   | &   | mask 00101 | 21  | 7   | mask 10101
6   | (   | mask 00110 | 22  | 8   | mask 10110
7   | )   | mask 00111 | 23  | 9   | mask 10111
8   | *   | mask 01000 | 24  | :   | mask 11000
9   | +   | mask 01001 | 25  | ;   | mask 11001
10  | ,   | mask 01010 | 26  | <   | mask 11010
11  | -   | mask 01011 | 27  | =   | mask 11011
12  | .   | mask 01100 | 28  | >   | mask 11100
13  | /   | mask 01101 | 29  | ?   | mask 11101
14  | 0   | mask 01110 | 30  | @   | mask 11110
15  | 1   | mask 01111 | 31  | A   | mask 11111

#### Grammar

<enum_mask> ::= '0'..'A'

### Message Modifiers

(used by: Messages)

Any encoded message may be immediately precesed by a message modifier which adds
semantic meaning to the message which follows it. A message modifier is encoded
as set of low-endian bitfields:

bit | modifier
:-- | :-------
0   | Strings must be validated for UTF-8 (only meaningful for String fields)
1   | Packed repeated fields by default (only meaningful for repeated field types)
2   | Message defines one or more extension ranges

#### Grammar

<message_modifier> ::= 'L'..'['

## Encodings

### Messages

<message_encoding> ::= <message_tag> <message_modifier>? <message_element> \
<message_modifier> ::= 'L'..'[' \
<message_element> ::= <skip>* <message_field> \
<message_field> ::= <field_type> <field_modifier>? \

val | ch  | meaning    | val | ch  | meaning    | val | ch  | meaning    | val | ch  | meaning    | val | ch  | meaning | val | ch  | meaning
:-- | :-- | :--------- | :-- | :-- | :--------- | :-- | :-- | :--------- | :-- | :-- | :--------- | :-- | :-- | :------ | :-- | :-- | :------
0   |     | field type | 16  | 2   | field type | 32  | B   | repeated   | 48  | R   | modifiers  | 64  | c   | skips   | 80  | s   | skips
1   | !   | field type | 17  | 3   | field type | 33  | C   | repeated   | 49  | S   | modifiers  | 65  | d   | skips   | 81  | t   | skips
2   | #   | field type | 18  | 4   | field type | 34  | D   | repeated   | 50  | T   | modifiers  | 66  | e   | skips   | 82  | u   | skips
3   | $   | field type | 19  | 5   | field type | 35  | E   | repeated   | 51  | U   | modifiers  | 67  | f   | skips   | 83  | v   | skips
4   | %   | field type | 20  | 6   | repeated   | 36  | F   | repeated   | 52  | V   | modifiers  | 68  | g   | skips   | 84  | w   | skips
5   | &   | field type | 21  | 7   | repeated   | 37  | G   | repeated   | 53  | W   | modifiers  | 69  | h   | skips   | 85  | x   | skips
6   | (   | field type | 22  | 8   | repeated   | 38  | H   | repeated   | 54  | X   | modifiers  | 70  | i   | skips   | 86  | y   | skips
7   | )   | field type | 23  | 9   | repeated   | 39  | I   | repeated   | 55  | Y   | modifiers  | 71  | j   | skips   | 87  | z   | skips
8   | *   | field type | 24  | :   | repeated   | 40  | J   | (reserved) | 56  | Z   | modifiers  | 72  | k   | skips   | 88  | {   | skips
9   | +   | field type | 25  | ;   | repeated   | 41  | K   | (reserved) | 57  | [   | modifiers  | 73  | l   | skips   | 89  | \|  | skips
10  | ,   | field type | 26  | <   | repeated   | 42  | L   | modifiers  | 58  | ]   | (reserved) | 74  | m   | skips   | 90  | }   | skips
11  | -   | field type | 27  | =   | repeated   | 43  | M   | modifiers  | 59  | ^   | end        | 75  | n   | skips   | 91  | ~   | skips
12  | .   | field type | 28  | >   | repeated   | 44  | N   | modifiers  | 60  | _   | skips      | 76  | o   | skips   |     |     |
13  | /   | field type | 29  | ?   | repeated   | 45  | O   | modifiers  | 61  | `   | skips      | 77  | p   | skips   |     |     |
14  | 0   | field type | 30  | @   | repeated   | 46  | P   | modifiers  | 62  | a   | skips      | 78  | q   | skips   |     |     |
15  | 1   | field type | 31  | A   | repeated   | 47  | Q   | modifiers  | 63  | b   | skips      | 79  | r   | skips   |     |     |

### Enums (Closed)

The encoding for an enum MiniDescriptor is the enum version character followed
by a series of skip values and/or enum masks which list the individual enum
values in
ascending order.

#### Grammar

<enum_encoding> ::= <enum_tag> <enum_element>* \
<enum_element> ::= <enum_mask> | <skip>

#### Examples

An enum with values of 3 and 4: !.
(Version followed by an enum mask with bits 2 and 3 set.)

An enum with values of 1 and 28 and 29: !1z3
(Version followed by an enum mask with bit 0 set follwed by a skip of 27
followed by a mask with bits 0 and 1 set.)

### Extensions

The encoding for an extension MiniDescriptor is the extension version character
followed by a field def which describes the extension type.

<extension_encoding> ::= <extension_tag> <field_type>

### Maps

The encoding for a map MiniDescriptor is the map version character followed by
the encoded field characters for the key and value types, in that order.

<map_encoding> ::= <map_tag> <key_field> <value_field> \
<key_field> ::= <singular_field> \
<value_field> ::= <field_type>

### Message Sets

The encoding for a message set MiniDescriptor is simply the message set version
character; no further payload is needed.

<message_set_encoding> ::= <message_set_tag>
