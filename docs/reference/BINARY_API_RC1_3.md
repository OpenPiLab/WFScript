# Native Binary API — WFScript v0.2 Binary Lab RC1.3

**Status:** development candidate, not a public release or a Windows-native-validated build. Requires the revised WFScript executable; opening the test Panel in an older executable does not add the `Binary` global.

## Model

WFScript retains JavaScript control of instruments, commands, protocol decisions, operators, and tests. Native Qt C++ objects own the bulk bytes and numeric samples. All binary objects run on the Panel's QJSEngine thread; **no external hardware or network connection is opened by `Binary` itself**.

```text
                        JavaScript instrument/test script
                              |             |
                         TCP/Serial/UDP   FileIO (permitted paths)
                              |             |
                        native QByteArray / BinaryBuffer
                              |
              BinaryStream     BinaryBuffer      BinarySamples
             framing,lines    fields,DBC,CRC     bulk DSP/plot bins
                              |
                  controlled small results back to JS
```

### Key rules

- `BinaryBuffer` holds **native bytes**, not a JavaScript array. `slice()` shares storage until a write causes Qt copy-on-write detachment. Mutating a view does not mutate the original or sibling views.
- Fixed-length buffers support both reading and writing. Use `Binary.create(size)` for a zero-filled packet and field write methods to populate it. Buffer size does not automatically grow.
- **Scalar native calls have overhead.** Our physical Windows QJSEngine microbenchmarks found 10,000 individual native field reads much slower than JavaScript indexing. For waveform-size data, use `statsNumeric()` or `decodeSamples()` once rather than a million `readU16LE()` calls.
- 64-bit integer *values* cross the JS bridge as **decimal strings**, preserving values beyond JavaScript's exact integer range. `readBits64/writeBits64` support full-width, unaligned DBC signals.
- `BinaryStream` parses received bytes without controlling TCP, SCPI commands, the instrument, or request timeouts. JavaScript chooses when the next response is a line, definite-length IEEE block, fixed frame, length-prefixed frame or delimiter frame.
- `TCP.on('binary')` produces **chunks**, not whole replies. Do not run two unrelated receive parsers on the same socket. Existing `TCP.on('data')` array mode is preserved.

## 1. Global `Binary` methods

| Method | Return / purpose |
|---|---|
| `Binary.create(size)` | Zero-filled writable `BinaryBuffer`, up to 8 MiB |
| `Binary.fromArray(bytes)` | Explicit number-array → native buffer copy |
| `Binary.fromHex(text)` | Hexadecimal → buffer, whitespace permitted |
| `Binary.fromUtf8(text)` | UTF-8 encoded bytes |
| `Binary.fromBase64(text)` | Strict Base64 decode to buffer |
| `Binary.concat([buffer,...])` | Concatenates native buffers; output has its own byte storage |
| `Binary.wrapIEEE(payload)` | IEEE 488.2 `#<n><length>` + payload + LF |
| `Binary.stream(maxBytes)` | Native `BinaryStream` receive accumulator |
| `Binary.pattern(size)` | Deterministic test bytes |
| `Binary.wordPattern(count)` | Deterministic little-endian 16-bit benchmark samples |
| `Binary.clockUs()` | Monotonic clock reading for benchmarks, microseconds |
| `Binary.lastError` | Factory-level error string |

## 2. `BinaryBuffer` reading and writing

For **each** of the following types, the buffer implements both `read<Type>(byteOffset)` and `write<Type>(byteOffset,value)`:

| Type suffix | Data type | Endianness | JS representation |
|---|---|---|---|
| `U8`, `I8` | Unsigned/signed 8-bit integer | N/A | Number |
| `U16LE`, `U16BE` | Unsigned 16-bit integer | Little / big | Number |
| `I16LE`, `I16BE` | Signed 16-bit integer | Little / big | Number |
| `U32LE`, `U32BE` | Unsigned 32-bit integer | Little / big | Number |
| `I32LE`, `I32BE` | Signed 32-bit integer | Little / big | Number |
| `F32LE`, `F32BE` | IEEE 754 32-bit float | Little / big | Number |
| `F64LE`, `F64BE` | IEEE 754 64-bit float | Little / big | Number |
| `U64LE`, `U64BE` | Unsigned 64-bit integer | Little / big | **Decimal string** |
| `I64LE`, `I64BE` | Signed 64-bit integer | Little / big | **Decimal string** |

Example:

```javascript
const packet=Binary.create(24);
if (!packet.writeU16BE(0,0x1234)) throw Error(packet.lastError);
packet.writeI16LE(2,-128);
packet.writeF32LE(4,3.125);
packet.writeF64BE(8,Math.PI);
packet.writeU64LE(16,'18446744073709551615');
print(packet.toHex(0,24));
print(packet.readU16BE(0),packet.readI16LE(2));
print(packet.readU64LE(16)); // exact decimal string
```

Additional methods:

| API | Purpose |
|---|---|
| `.length`, `.lastError` | Read-only view size and latest error |
| `.slice(offset,count)` | Copy-on-write native view |
| `.ieeePayload()` | Validate complete definite-length IEEE frame; return payload view |
| `.fill(byte,offset,count)` | Fill selected region with 0..255 byte |
| `.copyFrom(source,destOffset,sourceOffset,count)` | Copy bytes; overlapping ranges supported |
| `.writeUtf8(offset,text)` / `.toUtf8(offset,count)` | Write/read bounded UTF-8 text |
| `.toBase64()` / `.toHex(offset,count)` | Export data as encoded strings |
| `.toArray(offset,count)` | Explicit JS array export (maximum **4096 bytes**) |
| `.sum32()` / `.sum32Range(offset,count)` | Byte sum modulo 2³² |
| `.crc16()` / `.crc16Range(offset,count)` | Modbus CRC16 (0xA001 reflected, init 0xFFFF) |
| `.crc32()` / `.crc32Range(offset,count)` | CRC-32/ISO-HDLC, standard ZIP/Ethernet vector |
| `.xor8(offset,count)` | Bytewise XOR checksum |
| `.indexOf(needle,start)` | Native buffer search (result -1 if not found) |
| `.equals(otherBuffer)` | Exact byte-for-byte comparison |
| `.statsU16LE(offset,count)` | Efficient unsigned WORD min/max/mean/RMS |
| `.statsNumeric(type,offset,count,scale,bias)` | Scaled native statistics for typed sample data |
| `.decodeSamples(type,offset,count,scale,bias)` | Converts to a `BinarySamples` native numeric series |
| `.release()` | Release this wrapper's bytes (other shared views survive) |

A write returns `false` and sets `.lastError` for invalid bounds, out-of-range integers, nonfinite floats or other invalid input. Some reads use null/empty string/NaN or a documented sentinel; always inspect `.lastError` when input is untrusted. The buffer **never resizes implicitly**.

### Bulk sample types

`statsNumeric()` and `decodeSamples()` accept these case-insensitive strings:

`u8`, `i8`, `u16le`, `u16be`, `i16le`, `i16be`, `u32le`, `u32be`, `i32le`, `i32be`, `f32le`, `f32be`, `f64le`, `f64be`.

The conversion formula is `value = nativeRaw * scale + bias`. Nonfinite decoded/scaled values are rejected, and an individual bulk call is limited to **1,000,000 samples**.

### Example: SCPI WORD waveform

```javascript
// Received IEEE payload is native BinaryBuffer, not JS byte[].
const series=payload.decodeSamples('u16le',0,payload.length/2,
                                   preamble.yIncrement,
                                   -(preamble.yOrigin+preamble.yReference)*preamble.yIncrement);
if (!series) throw Error(payload.lastError);
const stats=series.stats();
const bins=series.minMaxBins(1200); // [{start,end,min,max}, ...]
print(stats.rms,bins.length);
series.release();
```

This example shows the *mechanical conversion* and uses the existing Rigol preamble formula convention. It does not prove a new MHO954 hardware acquisition or create a live chart connection.

## 3. CAN DBC field packing and unpacking

`BinaryBuffer` provides generic signal bit-field primitives for payloads with the two standard DBC bit-numbering conventions:

| API | Meaning |
|---|---|
| `.readBits(startBit,width,order,isSigned)` | Raw integer as JS Number, width **1..53** |
| `.writeBits(startBit,width,order,isSigned,raw)` | Write bounded raw integer as JS Number |
| `.readBits64(startBit,width,order,isSigned)` | Raw integer as **decimal string**, width **1..64** |
| `.writeBits64(startBit,width,order,isSigned,decimal)` | Exact full-width integer write |
| `.readSignal(startBit,width,order,isSigned,factor,bias)` | Raw field scaled to a physical engineering value |
| `.writeSignal(startBit,width,order,isSigned,factor,bias,physical)` | Quantize physical value to nearest raw integer and pack |

`order` accepts `intel` / `little` / `le` or `motorola` / `big` / `be`.

- Intel start bit 0 is the least-significant bit of the first byte, with contiguous increasing bit indices.
- Motorola uses DBC **sawtooth** indexing: bit 7 of byte 0 is the first MSB. Within a byte the bit indices decrease, then wrap to the next byte's bit 7.
- `factor` must be finite and nonzero. `physical = raw * factor + bias`. The write operation rounds `(physical-bias)/factor` to the nearest integer and rejects out-of-range values.
- The API modifies only bits belonging to the selected signal. It does **not** interpret DBC files, DBC multiplexors, value tables, CAN metadata, message IDs, cycle times, CRC/alive-counter conventions, or physical hardware CAN FD frames. It is a reliable bit-packing primitive, not a DBC library.

```javascript
const frame=Binary.create(8);
frame.writeBits(0,12,'intel',false,0xABC);
print(frame.toHex(0,2)); // bc0a
frame.fill(0,0,8);
frame.writeBits(7,12,'motorola',false,0xABC);
print(frame.toHex(0,2)); // abc0
frame.fill(0,0,8);
frame.writeSignal(0,16,'intel',true,0.1,-40,23.5);
print(frame.readSignal(0,16,'intel',true,0.1,-40)); // 23.5
frame.writeBits64(7,64,'motorola',true,'-123456789012345');
print(frame.readBits64(7,64,'motorola',true)); // exact signed decimal string
```

## 4. `BinaryStream` native receive framing

| API | Behavior |
|---|---|
| `.append(chunk)` | Append native bytes; returns false on a size error |
| `.pending`, `.lastError` | Pending byte count; parse error string |
| `.readLine()` | One LF-terminated UTF-8 line, CR optional, max 8192 bytes |
| `.readIEEE()` | Complete IEEE 488.2 definite-length `#1`–`#9` payload, or `null` while incomplete |
| `.readFixed(count)` | One fixed-length native frame, or `null` while incomplete |
| `.readLengthPrefixed(prefixBytes,order,includePrefix)` | Payload length prefix of 1/2/4 bytes, endian selection; returns requested body or full frame |
| `.readUntil(delimiter,includeDelimiter)` | Consume through an arbitrary native byte delimiter, optionally include it in output |
| `.clear()` | Drop buffered bytes and framing state |

**Example: mixed SCPI text and binary, one TCP socket** (illustrative session core):

```javascript
const rx=Binary.stream(4*1024*1024);
let expecting='text'; // SCPI request layer controls this
const tcp=TCP.create({host:'192.168.6.2',port:5555});
tcp.on('binary',function(chunk){
    if(!rx.append(chunk)) throw Error(rx.lastError);
    if(expecting==='text'){
        const line=rx.readLine();
        if(line!==null) print('Text SCPI reply:',line);
    }else{
        const payload=rx.readIEEE();
        if(payload){print('WORD bytes:',payload.length);payload.release();}
    }
    if(rx.lastError)throw Error(rx.lastError);
});
// A production SCPI session must serialize requests, account for all
// buffered responses, time out/recover, and never run competing readers.
```

The existing Rigol MHO954 JavaScript driver is **not migrated** in RC1.3. The tested v0.4.9 driver continues to use its known-good receive and verification logic until a controlled hardware comparison.

## 5. Native file and communication paths

The following **new** FileIO calls use the same validated `FileIO.dataPath()` / operator-granted path restrictions as ordinary FileIO and retain its **2 MiB per-file size limit**:

```javascript
const path=FileIO.dataPath('packet.bin');
const data=Binary.create(16);
data.writeU32BE(0,0x12345678);
FileIO.writeBuffer(path,data);  // atomic replacement
FileIO.appendBuffer(path,Binary.fromHex('aabbccdd')); // bounded append
const restored=FileIO.readBuffer(path);
print(restored.length,restored.readU32BE(0));
```

Existing FileIO text/JSON/CSV/byte-array methods are unchanged. This revision is **not a streaming large-file API**; files greater than 2 MiB remain disallowed.

Existing optional native communication paths:

| Path | Native buffers | Legacy compatibility |
|---|---|---|
| TCP | `.on('binary', callback)` receives native chunks; `.write(buffer)` sends | `on('data')` still emits JS `number[]` |
| Serial | Same binary event and `.write(buffer)` | Existing data and line events retained |
| UDP | `.on('binaryMessage', (buffer,remote) => ...)`, `.send(buffer,host,port)` | Existing message/text datagrams retained |

TCP/Serial per-write limit is **65,536 bytes**, outbound TCP queue **256 KiB**; UDP maximum datagram **65,507 bytes**. Larger payloads must be sent in bounded slices respecting write buffering and send completion. Setting an event handler does not connect a socket or drive an instrument output automatically.

## 6. `BinarySamples`: native decoded series

The result of `BinaryBuffer.decodeSamples(...)` stores numerical samples natively. Its API is:

| API | Behavior |
|---|---|
| `.length`, `.lastError` | Native sample count, last error |
| `.stats()` | `{count,min,max,sum,mean,rms}` in native C++ |
| `.minMaxBins(bins)` | Up to 2048 `{start,end,min,max}` groups, retains waveform extrema |
| `.toArray(offset,count)` | Explicit export of up to 4096 numeric values |
| `.at(index)` | Scalar read; avoid per-element calls in large loops |
| `.release()` | Free native numeric samples |

**Future chart integration:** the native series keeps the data and could later feed PlotBridge using an internal C++ path. RC1.3 does **not** add `plots.setNativeSamples` or direct plot memory ownership. `minMaxBins()` is a bounded intermediate representation for the current JavaScript side; for a true direct PlotBridge path we still need a compatible Qt GUI command and lifetime/thread ownership contract. That is intentionally deferred until this foundational API passes native tests.

## 7. Editor features

RC1.3 registers the **`Binary` global, its factory methods, all 64 buffer methods, 7 stream methods, 5 native-sample methods, and the new FileIO methods** with:

- completion catalog (methods and properties);
- F1 documentation including signatures, endian rules, return semantics and examples;
- API Explorer (`Binary` root plus non-insertable reference types `BinaryBuffer`, `BinaryStream`, `BinarySamples`);
- snippets: `binarypacket`, `binarydbc`, `binaryscpi`;
- static type inference for assignments from `Binary.*`, `b.slice()`, `b.ieeePayload()`, `b.decodeSamples()`, `stream.readIEEE/readFixed/readLengthPrefixed()`, and `FileIO.readBuffer()`.

The current editor does **not** yet infer the type of a callback parameter declared *inside* `tcp.on('binary', function(chunk){...})` automatically. For that case consult the `BinaryBuffer` type in API Explorer, or assign a typed factory result in a standalone test. F1 and completion for an assigned `BinaryBuffer` variable are implemented in this source candidate, pending Windows UI testing.

## 8. Limits, compatibility and verification

- Per native buffer and stream limit: **8 MiB**. This is a per-object bound, not a complete per-Panel memory budget.
- `statsNumeric`/`decodeSamples`: at most **1 million samples** per call. Exports `toArray`/`toHex`: at most 4096 elements/bytes. Plot bins: at most 2048.
- Copy-on-write buffers may allocate when modified while shared, and stream frame assembly may copy native bytes. "Native" here means eliminating per-byte **JavaScript** conversion, not claiming a universally zero-copy network stack.
- TCP SCPI binary data still requires proper transaction order, definite framing, timeouts and error handling, under JavaScript driver control.
- Legacy `TCP`, `Serial`, `UDP`, `FileIO` and `.wfpanel` schema remain unchanged. Existing 40 built-in examples are preserved.
- Offline source checks, Panel simulation harness, repository audits, and ZIP checks are distinct from native Qt compilation and the physical Analog Discovery 3 or Rigol MHO954 hardware tests.

Use `development/binary_lab/NativeBinaryPerformanceLab.wfpanel` for the **four-page** Windows test suite. Pages 1–2 preserve the RC1.2 SCPI/file/TCP benchmarks; Page 3 adds endian/64-bit/file/framing round trips; Page 4 adds CAN DBC wire-format and bulk sample/bin comparisons.
