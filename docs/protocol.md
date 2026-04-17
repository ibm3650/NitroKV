NitroKv supports simple and easy to understand protocol RESP2. Maybe in some moments you can collide  with some limitations
of my realization. But still i tried to make full support.


All items, includes prefix and every item separated by '\r\n' sequence. Double sequency unacceptable.


# Basic types:

## Simple string
Prefix: '+'
Item 0: payload, text, technically - any byte, except terminators
Grammar: All 0x00-0xFF inclusive, except terminal symbols \r (0x0D) or \n (0x0A)
Frequently uses for responses of service commands

## Bulk string
Prefix: '$'
Item 0: Length of data as integer 0-512*1024*1024
Item 1: Payload, raw binary data
Grammar: Any byte 0x00-0xFF. Is binary-safe format, parser doesn't respond on terminal sequences.
Uses for store any binary data, limited only by size

## Null Bulk String:
Prefix: '$'
Item 0: Length, reserved integer value -1
Is special case of Bulk string, in fact, that's what he is. NULL analogue

## Array:
Prefix: '*'
Item 0: Count of elements as integer
Item 1 - N: Any base type, elements in same array can be as any type. For concatenating uses default terminal sequence


## Null Array:
Prefix: '*'
Item 0: Length, reserved integer value -1
Is special case of Array, in fact, that's what he is. Is different state  unlike empty array


## Integer:
Prefix: ':'
Item 0: String representation of int64 value


## Error:
Prefix: '-'
Same as simple string, but with other prefix. frequently uses for raise exception

# Commands
PING

GET

SET

DEL

EXPIRE

TTL

STATS - TODO

EXISTS


# extras:
float type
UTF8 simple string
## Simple string ascii
Prefix: '+'
Item 0: payload, text, technically - any byte, except terminators
Grammar: All ascii symbols 32-126 inclusive, except terminal symbols \r (0x0D) or \n (0x0A)
Frequently uses for responses of service commands

