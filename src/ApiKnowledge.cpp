#include "ApiKnowledge.h"
#include "CompletionCatalog.h"
#include <QRegularExpression>
#include <QStringList>
#include <QHash>

namespace ApiKnowledge {
namespace {
struct Detail {
    const char* path;
    const char* signature;
    const char* parameters;
    const char* unit;
    const char* example;
    const char* notes;
};
// Curated detail is SUPPLEMENTAL. Symbol availability and the short summary
// always come from CompletionCatalog. Adding an API must update that inventory;
// the automated tests fail if a curated path no longer exists.
static const Detail kDetails[] = {
    {"Binary", "Native binary buffer service", "", "", "const frame=Binary.wrapIEEE(Binary.fromHex(\"01 02\"));", "Opt-in per Panel run. No I/O or instrument setup occurs automatically. BinaryBuffer uses bounded copy-on-write native storage."},
    {"Binary.create", "create(bytes) : BinaryBuffer", "0..8 MiB bytes", "", "const b=Binary.create(8); b.writeU16LE(0, 65535);", "Zero-filled writable native buffer. Buffers are fixed-length, views detach on write."},
    {"Binary.fromHex", "fromHex(hex) : BinaryBuffer", "Even number of hex digits; whitespace allowed", "", "const b=Binary.fromHex(\"12 34 ab cd\");", "Rejects non-hex characters and odd digit count."},
    {"Binary.fromArray", "fromArray(numbers) : BinaryBuffer", "Array of byte values 0..255", "", "const b=Binary.fromArray([0xAA,0x55]);", "Copies a JS array once into native bytes. Prefer native receive and native file read for large payloads."},
    {"Binary.fromUtf8", "fromUtf8(text) : BinaryBuffer", "JavaScript text string", "", "const b=Binary.fromUtf8(\"HELLO\\n\");", "UTF-8 encoding; not a SCPI framing parser."},
    {"Binary.fromBase64", "fromBase64(text) : BinaryBuffer", "Strict Base64 string", "", "const b=Binary.fromBase64(\"AQIDBA==\");", "Invalid Base64 is rejected; 8 MiB decoded limit."},
    {"Binary.concat", "concat(buffers) : BinaryBuffer", "Array of BinaryBuffer objects", "", "const b=Binary.concat([Binary.fromHex(\"01\"), Binary.fromHex(\"02\")]);", "Copies into one native buffer, up to 8 MiB. Slice instead for shared read-only views."},
    {"Binary.wrapIEEE", "wrapIEEE(payload) : BinaryBuffer", "BinaryBuffer payload", "", "const frame=Binary.wrapIEEE(Binary.fromHex(\"01 02\"));", "Encodes an IEEE 488.2 definite-length block with LF. Buffer, header and terminator are native."},
    {"Binary.stream", "stream(maxBytes) : BinaryStream", "Positive max bytes up to 8 MiB", "", "const rx=Binary.stream(4000000);", "Handles partial headers and payloads. JavaScript controls whether each response is a line, IEEE block, or another framing type."},
    {"Binary.pattern", "pattern(byteCount) : BinaryBuffer", "Byte count 0..8MiB", "", "const b=Binary.pattern(4096);", "Deterministic benchmark generator; not random test data."},
    {"Binary.wordPattern", "wordPattern(samples) : BinaryBuffer", "16-bit little-endian sample count", "", "const b=Binary.wordPattern(100000);", "Benchmark fixture only."},
    {"Binary.clockUs", "clockUs() : number", "No arguments", "", "const start=Binary.clockUs();", "Monotonic microseconds since Panel BinaryBridge creation; useful for timings, not instrument timestamps."},
    {"Binary.lastError", "lastError : string", "Read only", "", "if (!b) print(Binary.lastError);", "Factory-level validation failures."},
    {"BinaryBuffer.crc32", "crc32() : number", "Whole native buffer", "", "print(b.crc32());", "CRC-32/ISO-HDLC used in ZIP and Ethernet, with reflected polynomial 0xEDB88320, initial FFFFFFFF and final XOR FFFFFFFF."},
    {"BinaryBuffer.crc32Range", "crc32Range(offset,count) : number", "Byte region", "", "print(b.crc32Range(0,16));", "CRC-32 of selected bytes, not every possible industrial CRC-32 variant."},
    {"BinaryBuffer.indexOf", "indexOf(needle,start) : number", "BinaryBuffer needle and start offset", "", "print(b.indexOf(Binary.fromHex('aa55'),0));", "Search native binary data without converting to JavaScript byte arrays. Returns -1 if absent."},
    {"BinaryBuffer.equals", "equals(other) : boolean", "Other native BinaryBuffer", "", "print(a.equals(b));", "Byte-exact equality for complete native views; no JavaScript array transfer."},
    {"BinaryBuffer.readBits64", "readBits64(startBit,width,order,isSigned) : decimal string", "DBC bit position, 1..64 bits, intel/motorola, signed", "", "print(can.readBits64(0,64,'intel',false));", "Lossless signed or unsigned DBC bit extraction, using a decimal string rather than a JS Number."},
    {"BinaryBuffer.writeBits64", "writeBits64(startBit,width,order,isSigned,decimal) : boolean", "DBC bit position, 1..64 bits, intel/motorola, signed, decimal string", "", "can.writeBits64(0,64,'intel',false,'18446744073709551615');", "Lossless unaligned DBC bit-field write with exact 64-bit decimal input and range validation."},
    {"BinaryStream.readUntil", "readUntil(delimiter,includeDelimiter) : BinaryBuffer|null", "Nonempty native delimiter and boolean inclusion flag", "", "const msg=rx.readUntil(Binary.fromHex('00'),false);", "Native delimiter framing. Consumes delimiter even when omitted from returned frame; preserves following bytes."},
    {"BinaryBuffer.length", "length : integer", "Read only", "", "print(b.length);", "Bytes visible through the current native view."},
    {"BinaryBuffer.lastError", "lastError : string", "Read only", "", "if(!b.writeU8(100,1)) print(b.lastError);", "Bounds or conversion errors; returns false/null/sentinel as documented."},
    {"BinaryBuffer.slice", "slice(offset,count) : BinaryBuffer", "Byte offset and length", "", "const view=b.slice(4,8);", "Shares storage until modified. Writes detach the edited view via QByteArray copy-on-write; sibling views remain unchanged."},
    {"BinaryBuffer.ieeePayload", "ieeePayload() : BinaryBuffer", "Complete framed buffer", "", "const payload=frame.ieeePayload();", "Validates a definite-length IEEE block with optional LF/CRLF; returns native shared payload view."},
    {"BinaryBuffer.fill", "fill(byte,offset,count) : boolean", "Byte 0..255; bounded offset/count", "", "b.fill(0,0,b.length);", "Overwrites fixed-length bytes, detaching shared views."},
    {"BinaryBuffer.copyFrom", "copyFrom(source,destOffset,sourceOffset,count) : boolean", "BinaryBuffer source and bounded offsets", "", "b.copyFrom(header,0,0,header.length);", "Overlapping copies are safe. Existing buffer size is not changed."},
    {"BinaryBuffer.writeUtf8", "writeUtf8(offset,text) : boolean", "Byte offset and UTF-8 text", "", "b.writeUtf8(0,\"SCPI\\n\");", "Does not append or resize. Fails if encoded bytes exceed view length."},
    {"BinaryBuffer.toUtf8", "toUtf8(offset,count) : string", "Byte offset and length", "", "print(b.toUtf8(0,b.length));", "Decode UTF-8 text from a bounded byte region."},
    {"BinaryBuffer.toBase64", "toBase64() : string", "No args", "", "const b64=b.toBase64();", "Exports binary as Base64 text; creates a string, not zero-copy."},
    {"BinaryBuffer.toHex", "toHex(offset,count) : string", "At most 4096 bytes", "", "print(b.toHex(0,16));", "Small diagnostic export; returns lowercase continuous hex."},
    {"BinaryBuffer.toArray", "toArray(offset,count) : number[]", "At most 4096 bytes", "", "print(b.toArray(0,8));", "Explicit small JS conversion; not suitable for million-sample loops."},
    {"BinaryBuffer.release", "release() : void", "No arguments", "", "b.release();", "Drops native storage for this wrapper. Other shared views continue to own their storage."},
    {"BinaryBuffer.readU8", "readU8(offset) : number", "Byte offset within view", "", "const value=b.readU8(0);", "unsigned integer, 8 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeU8", "writeU8(offset,value) : boolean", "Byte offset and number", "", "b.writeU8(0,123);", "Writes unsigned integer, 8 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readI8", "readI8(offset) : number", "Byte offset within view", "", "const value=b.readI8(0);", "signed integer, 8 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeI8", "writeI8(offset,value) : boolean", "Byte offset and number", "", "b.writeI8(0,123);", "Writes signed integer, 8 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readU16LE", "readU16LE(offset) : number", "Byte offset within view", "", "const value=b.readU16LE(0);", "unsigned integer, 16 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeU16LE", "writeU16LE(offset,value) : boolean", "Byte offset and number", "", "b.writeU16LE(0,123);", "Writes unsigned integer, 16 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readU16BE", "readU16BE(offset) : number", "Byte offset within view", "", "const value=b.readU16BE(0);", "unsigned integer, 16 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeU16BE", "writeU16BE(offset,value) : boolean", "Byte offset and number", "", "b.writeU16BE(0,123);", "Writes unsigned integer, 16 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readI16LE", "readI16LE(offset) : number", "Byte offset within view", "", "const value=b.readI16LE(0);", "signed integer, 16 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeI16LE", "writeI16LE(offset,value) : boolean", "Byte offset and number", "", "b.writeI16LE(0,123);", "Writes signed integer, 16 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readI16BE", "readI16BE(offset) : number", "Byte offset within view", "", "const value=b.readI16BE(0);", "signed integer, 16 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeI16BE", "writeI16BE(offset,value) : boolean", "Byte offset and number", "", "b.writeI16BE(0,123);", "Writes signed integer, 16 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readU32LE", "readU32LE(offset) : number", "Byte offset within view", "", "const value=b.readU32LE(0);", "unsigned integer, 32 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeU32LE", "writeU32LE(offset,value) : boolean", "Byte offset and number", "", "b.writeU32LE(0,123);", "Writes unsigned integer, 32 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readU32BE", "readU32BE(offset) : number", "Byte offset within view", "", "const value=b.readU32BE(0);", "unsigned integer, 32 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeU32BE", "writeU32BE(offset,value) : boolean", "Byte offset and number", "", "b.writeU32BE(0,123);", "Writes unsigned integer, 32 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readI32LE", "readI32LE(offset) : number", "Byte offset within view", "", "const value=b.readI32LE(0);", "signed integer, 32 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeI32LE", "writeI32LE(offset,value) : boolean", "Byte offset and number", "", "b.writeI32LE(0,123);", "Writes signed integer, 32 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readI32BE", "readI32BE(offset) : number", "Byte offset within view", "", "const value=b.readI32BE(0);", "signed integer, 32 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeI32BE", "writeI32BE(offset,value) : boolean", "Byte offset and number", "", "b.writeI32BE(0,123);", "Writes signed integer, 32 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readF32LE", "readF32LE(offset) : number", "Byte offset within view", "", "const value=b.readF32LE(0);", "IEEE floating-point, 32 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeF32LE", "writeF32LE(offset,value) : boolean", "Byte offset and number", "", "b.writeF32LE(0,3.125);", "Writes IEEE floating-point, 32 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readF32BE", "readF32BE(offset) : number", "Byte offset within view", "", "const value=b.readF32BE(0);", "IEEE floating-point, 32 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeF32BE", "writeF32BE(offset,value) : boolean", "Byte offset and number", "", "b.writeF32BE(0,3.125);", "Writes IEEE floating-point, 32 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readF64LE", "readF64LE(offset) : number", "Byte offset within view", "", "const value=b.readF64LE(0);", "IEEE floating-point, 64 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeF64LE", "writeF64LE(offset,value) : boolean", "Byte offset and number", "", "b.writeF64LE(0,3.125);", "Writes IEEE floating-point, 64 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readF64BE", "readF64BE(offset) : number", "Byte offset within view", "", "const value=b.readF64BE(0);", "IEEE floating-point, 64 bits. LE=little-endian; BE=big-endian. Avoid per-sample JNI-like crossings; use decodeSamples() for bulk data."},
    {"BinaryBuffer.writeF64BE", "writeF64BE(offset,value) : boolean", "Byte offset and number", "", "b.writeF64BE(0,3.125);", "Writes IEEE floating-point, 64 bits. Integer range and bounds checked; buffer uses copy-on-write. "},
    {"BinaryBuffer.readU64LE", "readU64LE(offset) : decimal string", "Byte offset within view", "", "const value=b.readU64LE(0);", "unsigned integer, 64 bits. LE=little-endian; BE=big-endian. Returns decimal string to preserve full 64-bit precision."},
    {"BinaryBuffer.writeU64LE", "writeU64LE(offset,value) : boolean", "Byte offset and decimal string", "", "b.writeU64LE(0,\"18446744073709551615\");", "Writes unsigned integer, 64 bits. Integer range and bounds checked; buffer uses copy-on-write. 64-bit integer accepts a decimal string, never a rounded JavaScript Number."},
    {"BinaryBuffer.readU64BE", "readU64BE(offset) : decimal string", "Byte offset within view", "", "const value=b.readU64BE(0);", "unsigned integer, 64 bits. LE=little-endian; BE=big-endian. Returns decimal string to preserve full 64-bit precision."},
    {"BinaryBuffer.writeU64BE", "writeU64BE(offset,value) : boolean", "Byte offset and decimal string", "", "b.writeU64BE(0,123);", "Writes unsigned integer, 64 bits. Integer range and bounds checked; buffer uses copy-on-write. 64-bit integer accepts a decimal string, never a rounded JavaScript Number."},
    {"BinaryBuffer.readI64LE", "readI64LE(offset) : decimal string", "Byte offset within view", "", "const value=b.readI64LE(0);", "signed integer, 64 bits. LE=little-endian; BE=big-endian. Returns decimal string to preserve full 64-bit precision."},
    {"BinaryBuffer.writeI64LE", "writeI64LE(offset,value) : boolean", "Byte offset and decimal string", "", "b.writeI64LE(0,\"-9223372036854775808\");", "Writes signed integer, 64 bits. Integer range and bounds checked; buffer uses copy-on-write. 64-bit integer accepts a decimal string, never a rounded JavaScript Number."},
    {"BinaryBuffer.readI64BE", "readI64BE(offset) : decimal string", "Byte offset within view", "", "const value=b.readI64BE(0);", "signed integer, 64 bits. LE=little-endian; BE=big-endian. Returns decimal string to preserve full 64-bit precision."},
    {"BinaryBuffer.writeI64BE", "writeI64BE(offset,value) : boolean", "Byte offset and decimal string", "", "b.writeI64BE(0,123);", "Writes signed integer, 64 bits. Integer range and bounds checked; buffer uses copy-on-write. 64-bit integer accepts a decimal string, never a rounded JavaScript Number."},
    {"BinaryBuffer.sum32", "sum32() : number", "Whole buffer", "", "print(b.sum32());", "Unsigned 32-bit byte sum, modulo 2^32."},
    {"BinaryBuffer.sum32Range", "sum32Range(offset,count) : number", "Byte offset and length", "", "print(b.sum32Range(0,8));", "Subset checksum. Returns -1 on bad range."},
    {"BinaryBuffer.crc16", "crc16() : integer", "Whole buffer", "", "print(b.crc16());", "Modbus CRC16 polynomial 0xA001, seed FFFF, no swap/append automatically."},
    {"BinaryBuffer.crc16Range", "crc16Range(offset,count) : integer", "Byte offset and length", "", "print(b.crc16Range(0,8));", "CRC of specified bytes, initial FFFF. Caller chooses coverage and endian placement."},
    {"BinaryBuffer.xor8", "xor8(offset,count) : integer", "Byte offset and length", "", "print(b.xor8(0,8));", "8-bit XOR checksum over the selected bytes."},
    {"BinaryBuffer.readBits", "readBits(startBit,width,order,isSigned) : number", "Bit start, width 1..53, intel/motorola, signed bool", "", "const raw=b.readBits(7,12,\"motorola\",false);", "DBC bit numbering: Intel start is LSB and increments; Motorola start is MSB with sawtooth indexing. 53-bit max ensures exact JS integers. No DBC-file parsing or multiplexing."},
    {"BinaryBuffer.writeBits", "writeBits(startBit,width,order,isSigned,raw) : boolean", "Bit start, width 1..53, endian, signed, integer raw value", "", "b.writeBits(0,12,\"intel\",false,0xABC);", "Only selected bits are modified. Checks bounds and representable signed/unsigned range."},
    {"BinaryBuffer.readSignal", "readSignal(startBit,width,order,isSigned,factor,offset) : number", "DBC bits, signed, nonzero factor, physical offset", "", "const c=b.readSignal(7,12,\"motorola\",true,0.1,-40);", "Scales raw DBC value with physical = raw*factor + offset. Does not parse a DBC database file."},
    {"BinaryBuffer.writeSignal", "writeSignal(startBit,width,order,isSigned,factor,offset,physical) : boolean", "DBC bits, signed, scale, offset, physical value", "", "b.writeSignal(0,16,\"intel\",false,0.1,-40,23.5);", "Quantizes physical to nearest representable raw integer. Validates field bounds and scale."},
    {"BinaryBuffer.statsU16LE", "statsU16LE(byteOffset,count) : object", "Offset and 16-bit sample count", "", "const s=b.statsU16LE(0,100000);", "Native min/max/count/sum/mean/RMS for unsigned WORD samples, no JavaScript array allocation."},
    {"BinaryBuffer.statsNumeric", "statsNumeric(type,offset,count,scale,bias) : object", "Type u8/i8/u16le/be/i16le/be/u32le/be/i32le/be/f32le/be/f64le/be", "", "const s=b.statsNumeric(\"i16be\",0,1000,0.001,0);", "Native bulk scaled statistics. Avoid per-sample method calls. For plot-ready downsampling use decodeSamples()."},
    {"BinaryBuffer.decodeSamples", "decodeSamples(type,offset,count,scale,bias) : BinarySamples", "Type, byte offset, count up to 1M, scale, bias", "", "const data=b.decodeSamples(\"u16le\",0,100000,0.001,-2);", "Decodes numeric samples into native double storage. Does not yet transfer directly to plots; minMaxBins() provides bounded output for plots."},
    {"BinaryStream.pending", "pending : integer", "Read only", "", "print(rx.pending);", "Native bytes currently buffered."},
    {"BinaryStream.lastError", "lastError : string", "Read only", "", "if(rx.lastError) print(rx.lastError);", "Incomplete data can return null without error. Malformed frames set lastError."},
    {"BinaryStream.append", "append(buffer) : boolean", "BinaryBuffer chunk", "", "rx.append(chunk);", "Copies native bytes into accumulator, never into a JavaScript array. Bounded by stream maximum."},
    {"BinaryStream.readLine", "readLine() : string|null", "No args", "", "const status=rx.readLine();", "LF-delimited UTF-8 text, strips optional CR, max 8192 bytes. Preserves extra bytes."},
    {"BinaryStream.readIEEE", "readIEEE() : BinaryBuffer|null", "No args", "", "const payload=rx.readIEEE();", "Parses definite-length #1..#9 blocks; supports partial headers/body and optional CR/LF suffix. No SCPI command logic."},
    {"BinaryStream.readFixed", "readFixed(count) : BinaryBuffer|null", "Fixed frame byte count", "", "const payload=rx.readFixed(8);", "Returns null if incomplete; copies a native fixed-length frame, keeping subsequent bytes."},
    {"BinaryStream.readLengthPrefixed", "readLengthPrefixed(prefixBytes,order,includePrefix) : BinaryBuffer|null", "Prefix 1/2/4 bytes, little/big, include prefix bool", "", "const packet=rx.readLengthPrefixed(2,\"big\",false);", "Prefix is payload length; rejects lengths exceeding stream maximum. Packet boundaries independent of TCP read events."},
    {"BinaryStream.clear", "clear() : void", "No args", "", "rx.clear();", "Discards buffered state. Use on protocol resynchronization or deliberate stop."},
    {"BinarySamples.length", "length : integer", "Read only", "", "print(data.length);", "Decoded numeric sample count stored natively as double values."},
    {"BinarySamples.lastError", "lastError : string", "Read only", "", "print(data.lastError);", "Most recent samples operation failure."},
    {"BinarySamples.stats", "stats() : object", "No arguments", "", "const summary=data.stats();", "Native min/max/count/sum/mean/RMS. Nonfinite raw input is rejected during decode."},
    {"BinarySamples.minMaxBins", "minMaxBins(bins) : array", "1..2048 bins", "", "const bins=data.minMaxBins(1200);", "Returns up to 2048 objects {start,end,min,max}, preserving extrema per bin. Suitable for future plotting API; no direct plot integration yet."},
    {"BinarySamples.toArray", "toArray(offset,count) : number[]", "At most 4096 samples", "", "const short=data.toArray(0,100);", "Explicit limited JS number array export. Not efficient for multi-megasample conversion."},
    {"BinarySamples.at", "at(index) : number", "Index", "", "print(data.at(0));", "Convenience method; 10,000 individual QObject calls can be slower than JS array accesses."},
    {"BinarySamples.release", "release() : void", "No arguments", "", "data.release();", "Drop native sample data; runtime also cleans objects on Stop."},
    {"FileIO.readBuffer", "readBuffer(path) : BinaryBuffer", "Allowed PanelData path or Tool-granted path", "", "const b=FileIO.readBuffer(FileIO.dataPath(\"packet.bin\"));", "Reads up to 2 MiB directly into BinaryBuffer. Identical access checks as readBytes; no arbitrary file access."},
    {"FileIO.writeBuffer", "writeBuffer(path,buffer) : boolean", "Allowed path, BinaryBuffer", "", "FileIO.writeBuffer(FileIO.dataPath(\"packet.bin\"),b);", "Uses atomic QSaveFile save. Keeps 2 MiB limit and permission model; avoids numeric JS arrays."},
    {"FileIO.appendBuffer", "appendBuffer(path,buffer) : boolean", "Allowed path, BinaryBuffer", "", "FileIO.appendBuffer(FileIO.dataPath(\"log.bin\"),b);", "Appends native bytes with total file size <=2 MiB; obeys same FileIO access rules."},
    {"TCPConnection.on", "on(event,callback) : this", "data (legacy byte[]), binary (native BinaryBuffer), line, connect, error, close", "", "c.on(\"binary\",function(chunk){rx.append(chunk);});", "Events are opt-in. TCP is a byte stream; callback chunks do not equal messages. Use Binary.stream() to frame bytes. Existing data handlers remain compatible."},
    {"SerialConnection.on", "on(event,callback) : this", "data, binary, line, open, error, close", "", "serial.on(\"binary\",function(chunk){print(chunk.length);});", "binary callbacks receive native BinaryBuffer. Use BinaryStream for framing; legacy data events retained."},
    {"UDPConnection.on", "on(event,callback) : this", "message, binaryMessage, text, listening, error, close", "", "udp.on(\"binaryMessage\",function(data,peer){print(peer.port,data.length);});", "UDP datagram boundaries are preserved. binaryMessage carries native buffer + sender info; legacy message remains."},
    {"TCPConnection.write", "write(bytesOrBuffer) : number", "String, array of bytes, or native BinaryBuffer", "", "tcp.write(Binary.fromHex(\"010203\"));", "Writes up to 65536 bytes per call, bounded 256 KiB queue. Native write does not create a JS byte array. TCP is still a byte stream."},
    {"SerialConnection.write", "write(bytesOrBuffer) : number", "String, array, or BinaryBuffer", "", "serial.write(Binary.fromHex(\"AA55\"));", "Writes up to 65536 bytes per call. No implicit serial port open."},
    {"UDPConnection.send", "send(bytesOrBuffer,host,port) : number", "String, byte[], or BinaryBuffer", "", "udp.send(Binary.fromHex(\"AA55\"),\"127.0.0.1\",9000);", "UDP has 65507-byte max datagram and preserves message boundaries."},
    {"Data.get", "get(name) : number|boolean|string", "Declared Panel Data name", "", "const speed=Data.get('Motor.ActualRPM');", "Project-owned shared variables span all Pages. Unknown name throws. Values reset to saved initial defaults each Run; no automatic hardware operation or disk logging."},
    {"Data.set", "set(name,value) : boolean", "Declared name and value matching Number/Boolean/Text type", "", "Data.set('Motor.ActualRPM',1250);", "Automatically refreshes every compatible Value-bound widget on all Pages, without triggering a user event. Invalid type or missing name throws. Direct ui.set on a bound widget throws; for unbound widgets ui.set is unchanged."},
    {"Data.names", "names() : string[]", "No arguments", "", "print(Data.names().join(', '));", "Lists variables defined under Properties → Panel Data, not files in the shared PanelData directory. Design-time initial values and live Run values are separate."},
    {"Data.onChange", "onChange(name,callback,{immediate?:boolean}) : number", "Declared variable name, function(value,previousValue,name), optional immediate", "", "const watcher=Data.onChange('Motor.ActualRPM',function(value,previous,name){ print(name,previous,'->',value); },{immediate:true});", "Fires synchronously after successful, changed Data.set or compatible operator Value Binding edits. No notification for equal writes, initial project defaults, or invalid values. immediate:true invokes once on registration with previousValue undefined. Listeners may call Data.set; queued delivery avoids recursive stack growth and caps a feedback cycle at 256 events. Throws for invalid arguments or when 256 listeners are active. Callback errors appear in Panel diagnostics without stopping other subscribers. Returns an integer subscription ID valid until Stop."},
    {"Data.offChange", "offChange(subscriptionId) : boolean", "Numeric subscription ID from Data.onChange", "", "Data.offChange(watcher);", "Removes a previously registered listener. Returns true only once; false for unknown/removed IDs. Subscriptions are cleared automatically when the Panel stops. No physical outputs or I/O occur automatically."},
    {"Data.computed", "computed(name, dependencies, formula) : string", "Predeclared target variable; array of 1..16 declared dependency names; pure function(...values)", "", "// Declare Motor.TargetRPS as Number in Properties → Panel Data first.\nData.computed('Motor.TargetRPS',['Motor.TargetRPM'],function(rpm){return rpm/60;});", "Registers one typed, read-only runtime formula. Recomputes synchronously in topological order when any declared dependency changes, including bound operator inputs. Values propagate to bound displays and Data.onChange; source listeners see updated derived values. Dependencies must be explicit; formulas cannot call Data.set or register additional computed values. Up to 64 formulas. Duplicate targets, missing dependencies, editable output bindings and dependency cycles are rejected. Formula exceptions/invalid outputs are reported; previous derived values are retained. All computed definitions reset on Stop."},
    {"Data.isComputed", "isComputed(name) : boolean", "Declared variable name", "", "print(Data.isComputed('Motor.TargetRPS'));", "Returns true if Data.computed registered the declared variable for this Panel run. Computed values are read-only via Data.set and native input. Unknown names throw. All definitions must still exist in Properties → Panel Data to allow design-time widget binding."},
    {"Serial.ports", "ports() : array", "No arguments", "", "print(JSON.stringify(Serial.ports()));", "Native PC serial ports via QSerialPortInfo, not Analog Discovery 3 DIO. Returns objects with port, description, manufacturer, systemLocation, serialNumber. No port opened."},
    {"Serial.create", "create({port,baudRate?,dataBits?,parity?,stopBits?,flowControl?}) : SerialConnection", "port required; baud 50..4000000, bits 5..8, parity none/even/odd; stop 1/2; flow none/hardware/software", "", "const port=Serial.create({port:'COM5',baudRate:115200});\nport.on('line',line=>print(line));\nport.on('error',err=>print(err));\nport.open();", "Creates an unopened native QSerialPort. Connect physical TX/RX with appropriate interface or use a virtual COM-port pair for a software-only test. No automatic hardware action."},
    {"TCP.create", "create({host,port}) : TCPConnection", "Destination DNS hostname or IP; TCP port 1..65535", "", "const c=TCP.create({host:'127.0.0.1',port:5025});\nc.on('connect',()=>c.write('*IDN?\\n'));\nc.on('line',line=>print(line));\nc.connect();", "TCP client based on QTcpSocket, event-driven. write() accepts UTF-8 string or number[] bytes. Data events may contain partial messages; on('line') assembles LF lines."},
    {"TCP.createServer", "createServer({host?,port?}) : TCPServer", "Defaults localhost / ephemeral port 0", "", "const server=TCP.createServer({host:'127.0.0.1',port:0});\nserver.on('connection',c=>c.on('data',bytes=>c.write(bytes)));\nserver.listen();\nprint(server.info().localPort);", "Native QTcpServer. Explicit listen. Binding to 0.0.0.0 exposes the socket on the network; localhost is the safe default. TCP client event receives a full connection wrapper."},
    {"UDP.create", "create({host?,port?}) : UDPConnection", "Default bound address 127.0.0.1, port 0", "", "const u=UDP.create({host:'127.0.0.1',port:0});\nu.on('message',(bytes,from)=>print(bytes,from.port));\nu.bind();", "Native QUdpSocket datagrams; message(bytes,remote) emits complete byte arrays and sender {address,port}. text(text,remote) decodes UTF-8 per datagram."},
    {"SerialConnection.open", "open() : this", "No arguments", "", "port.open();", "Explicitly opens the COM port. open callback runs on the next event-loop turn; native open failure throws. close() releases it."},
    {"SerialConnection.off", "off(event) : this", "Event name", "", "port.off('line');", "Removes all listeners for the specified event; no implicit close."},
    {"SerialConnection.close", "close() : boolean", "No arguments", "", "port.close();", "Closes the native handle; pending callbacks are disabled on Panel shutdown."},
    {"SerialConnection.info", "info() : object", "No arguments", "", "print(port.info().bytesReceived);", "Object: valid,open,kind,bytesSent,bytesReceived,localPort,peerPort."},
    {"TCPConnection.connect", "connect() : this", "No arguments", "", "client.on('connect',()=>client.write('HELLO\\n')); client.connect();", "Nonblocking QTcpSocket.connectToHost. connection/error events arrive asynchronously; no timer polling."},
    {"TCPConnection.close", "close() : boolean", "No arguments", "", "client.close();", "Aborts and releases socket. Default server is bound to localhost until explicitly configured otherwise."},
    {"TCPConnection.info", "info() : object", "No arguments", "", "print(client.info().localPort);", "Read-only state, peer/local ports and byte counts."},
    {"TCPServer.on", "on('connection',callback) : this", "callback receives TCPConnection object", "", "server.on('connection',function(peer){peer.on('line',s=>peer.write(s+'\\n'));});", "Incoming QTcpSocket is assigned an independent connection object. Register its events inside the handler."},
    {"TCPServer.listen", "listen() : this", "No arguments", "", "server.listen();print(server.info().localPort);", "Only listens when invoked. port:0 allows OS to choose an ephemeral port for local self-tests."},
    {"TCPServer.close", "close() : boolean", "No arguments", "", "server.close();", "Stops listening. Already-accepted connections have their own handles and close automatically at Panel shutdown."},
    {"TCPServer.info", "info() : object", "No arguments", "", "print(server.info().localPort);", "Returns bound local TCP port, open/listening state and counters."},
    {"UDPConnection.bind", "bind() : this", "No arguments", "", "udp.bind();print(udp.info().localPort);", "Opt-in bind to localhost port 0 by default. To accept traffic from other hosts, explicitly set host:'0.0.0.0'."},
    {"UDPConnection.close", "close() : boolean", "No arguments", "", "udp.close();", "Stops receiving and releases the socket. Automatic cleanup on Panel shutdown."},
    {"UDPConnection.info", "info() : object", "No arguments", "", "print(udp.info().localPort);", "Returns local bound UDP port and byte counters."},
    {"Database.open", "open(filename,callback) : requestId", "Simple database filename (no folders); callback(result) with {ok,db,path,error}", "", "Database.open('lab.db',function(r){if(r.ok)print('Opened',r.db);else print(r.error);});", "Optional Qt QSQLITE driver required. Asynchronous on a dedicated database thread. Creates database in shared PanelData directory; other trusted Panels may open the same database file. At most 4 databases per Panel. Nothing opens automatically on startup."},
    {"Database.exec", "exec(db,sql,params,callback) : requestId", "Open database handle, SQL, parameter array, callback(result)", "", "Database.exec(db,'INSERT INTO samples(t,v) VALUES (?,?)',[Date.now(),3.30],function(r){print(r.ok,r.changes);});", "Prepared SQL binds parameters (not string concatenation). Supports INSERT/UPDATE/DELETE and ordinary CREATE/DROP/ALTER TABLE or INDEX. Only one statement per request; no ATTACH, PRAGMA, VACUUM, triggers or multi-statement SQL. Callback result {ok,changes,insertId,error}."},
    {"Database.query", "query(db,selectSql,params,callback) : requestId", "Open handle, SELECT statement, bound parameters and callback", "", "Database.query(db,'SELECT t,v FROM samples ORDER BY t DESC LIMIT 20',[],function(r){if(r.ok)ui.set('resultsTable',r.rows);});", "Callback {ok,columns,rows,rowCount,error}, where rows are actual JavaScript arrays of arrays. Max 250 rows / 16 columns, text cell limit 4096 chars. Use the existing Experiment Data Table widget with matching column headers."},
    {"Database.close", "close(db,callback) : requestId", "Handle returned by open; callback({ok,error})", "", "Database.close(db,function(r){print('Closed',r.ok);});", "Connections and pending JavaScript callbacks are cleaned up automatically on Panel shutdown. Queries and writes are queued and serialized on a dedicated QThread, not the AD3 worker."},
    {"FileIO.dataDirectory", "dataDirectory() : string", "No arguments", "path", "print(FileIO.dataDirectory());", "Returns the shared AppData/PanelData folder used by every Panel for FileIO and SQLite. File operations still require a file path; do not pass the directory itself."},
    {"FileIO.dataPath", "dataPath(filename) : string", "Relative filename inside shared PanelData directory", "path", "const path=FileIO.dataPath('measurements.csv');", "Returns an approved path; rejects absolute paths and parent traversal. All trusted Panels may read and write files in this shared location; choose unique filenames to prevent collisions."},
    {"FileIO.exists", "exists(path) : boolean", "Approved path", "", "print(FileIO.exists(FileIO.dataPath('log.csv')));", "Only Panel data paths and operator-granted paths are accessible."},
    {"FileIO.readText", "readText(path) : string", "Approved file path", "UTF-8", "const s=FileIO.readText(FileIO.dataPath('log.csv'));", "2 MiB limit; errors throw; synchronous, so use only for small files."},
    {"FileIO.writeText", "writeText(path,text) : boolean", "Approved file path; UTF-8 text", "UTF-8", "FileIO.writeText(FileIO.dataPath('notes.txt'),'hello');", "Atomic replacement using QSaveFile, 2 MiB limit; parent folder must exist."},
    {"FileIO.appendText", "appendText(path,text) : boolean", "Approved path and text", "UTF-8", "FileIO.appendText(FileIO.dataPath('log.csv'),'0.1,3.30\\n');", "Bounded append for small log files. Not atomic relative to crashes; 2 MiB cap."},
    {"FileIO.readJson", "readJson(path) : object|array", "Approved JSON file", "JSON", "const cfg=FileIO.readJson(FileIO.dataPath('config.json'));", "Throws on invalid JSON. Top-level object or array."},
    {"FileIO.writeJson", "writeJson(path,objectOrArray) : boolean", "JSON-compatible object or array", "JSON", "FileIO.writeJson(FileIO.dataPath('config.json'),{rate:100});", "Atomic replacement, pretty JSON. No file written until called."},
    {"FileIO.readCsv", "readCsv(path) : array[]", "Approved CSV file", "CSV", "const rows=FileIO.readCsv(FileIO.dataPath('samples.csv'));", "Quoted comma, embedded newline and escaped quote support; returns strings."},
    {"FileIO.writeCsv", "writeCsv(path,rows) : boolean", "Array of row arrays", "CSV", "FileIO.writeCsv(FileIO.dataPath('samples.csv'),[['time','V'],[0,3.3]]);", "Atomic replacement. Each CSV cell is quoted as required."},
    {"FileIO.readBytes", "readBytes(path) : number[]", "Approved binary file", "bytes", "const bytes=FileIO.readBytes(FileIO.dataPath('raw.bin'));", "Returns unsigned byte values 0..255; 2 MiB limit."},
    {"FileIO.writeBytes", "writeBytes(path,bytes) : boolean", "Approved file and array of unsigned bytes", "bytes", "FileIO.writeBytes(FileIO.dataPath('raw.bin'),[0,1,255]);", "Atomic binary write. Rejects invalid byte values."},
    {"Tool.question", "question(message,callback) : requestId", "Prompt string; callback({accepted,value,kind})", "", "Tool.question('Continue?',function(r){if(r.accepted)print('yes');});", "Asynchronous Qt dialog; callback runs on Panel QJSEngine thread. No modal wait in JavaScript."},
    {"Tool.getText", "getText(message,initial,callback) : requestId", "Prompt, initial text, callback(result)", "", "Tool.getText('UUT ID','',function(r){if(r.accepted)print(r.value);});", "Operator edit; cancellation gives accepted=false."},
    {"Tool.getNumber", "getNumber(message,initial,min,max,callback) : requestId", "Prompt, initial, minimum, maximum, callback", "", "Tool.getNumber('Voltage',3.3,0,5,function(r){print(r.value);});", "Native Qt floating input, not blocking JS."},
    {"Tool.getItem", "getItem(message,items,callback) : requestId", "Prompt, array of options, callback", "", "Tool.getItem('Range',['5V','50V'],function(r){print(r.value);});", "Selection dialog."},
    {"Tool.getOpenFile", "getOpenFile(title,filter,callback) : requestId", "Title, Qt file filter, callback", "", "Tool.getOpenFile('Import','CSV (*.csv)',function(r){if(r.accepted)print(FileIO.readCsv(r.value));});", "Explicitly grants read permission to selected file for current Panel run."},
    {"Tool.getSaveFile", "getSaveFile(title,filter,callback) : requestId", "Title, Qt file filter, callback", "", "Tool.getSaveFile('Export','CSV (*.csv)',function(r){if(r.accepted)FileIO.writeCsv(r.value,[[1,2]]);});", "Explicitly grants read/write permission for chosen file for current Panel run."},
    {"setInterval", "setInterval(callback, milliseconds, ...args) : integer", "Callable function; integer interval 10..86400000 ms; optional callback arguments", "ms", "let id = setInterval(function(){ ui.set('counter',Number(ui.get('counter'))+1); }, 250);\n// Later: clearInterval(id);", "Panel scripts only. A native Qt QTimer on the PanelRuntime/QJSEngine thread schedules repeat callbacks; each call creates an independent timer. At most 256 active timers. No overlap: long synchronous JavaScript delays this timer, UART polling and TestRunner. Invalid delays and over-capacity requests throw script errors. Not a real-time clock."},
    {"clearInterval", "clearInterval(timerId) : void", "Timer ID returned by setInterval(); an unknown ID is harmless", "", "const id=setInterval(function(){print('tick');},1000);\nclearInterval(id);", "Cancels a repeating timer immediately; can be called inside its callback. All timers are cancelled when the Panel stops. Cancelled IDs are not reused in the same Panel run."},
    {"setTimeout", "setTimeout(callback, milliseconds, ...args) : integer", "Callable function; integer delay 10..86400000 ms; optional callback arguments", "ms", "setTimeout(function(message){print(message);}, 500, 'ready');", "Panel scripts only. Schedules one callback; removed from active timers before invocation. Uses Qt event-loop timing; no guaranteed execution deadline. Does not block onInitialize() or other scripts. An invalid delay throws a script error."},
    {"clearTimeout", "clearTimeout(timerId) : void", "Timer ID returned by setTimeout(); an unknown ID is harmless", "", "const id=setTimeout(function(){print('late');},2000);\nclearTimeout(id);", "Cancels an outstanding one-shot timer. Safe when called after it has already fired. Automatic cleanup prevents callbacks after Panel stop and restart."},
    {"Protocol1.stop", "stop() : boolean", "No arguments", "", "if(!Protocol1.stop()) throw Error(Protocol1.lastError);", "Releases the protocol-owned DWF digital output engine and stops digital input. Never silently displaces Patterns, Logic or StaticIO. Always call stop in onShutdown or use panel native cleanup."},
    {"Protocol1.status", "status() : object", "No arguments", "", "print(Protocol1.status().activeProtocol);", "Reports active native protocol and configured states; Simulation has no hardware activity."},
    {"Protocol1.UART.configure", "configure({tx,rx,baudRate,dataBits,parity,stopBits,inverted}) : boolean", "tx/rx distinct DIO 0..15; parity none/even/odd/mark/space; stopBits 0.5..2", "baud", "if(!Protocol1.UART.configure({tx:0,rx:1,baudRate:115200,dataBits:8,parity:'none',stopBits:1})) throw Error(Protocol1.UART.lastError);", "Uses native FDwfDigitalUart* functions. Requires common GND and safe 3.3V logic levels. Separate RX initialization via startRx/Receiver; no automatic UART line decoder."},
    {"Protocol1.UART.startRx", "startRx() : boolean", "No arguments", "", "Protocol1.UART.startRx();", "Initializes native receiver with a zero-length buffer; then poll read()/ReceiveArray()."},
    {"Protocol1.UART.write", "write(bytes) : {ok,...}", "Array of integers 0..255", "bytes", "var tx=Protocol1.UART.write([0x55,0xAA,0]); if(!tx.ok) throw Error(tx.error);", "Binary-safe, sends zero bytes; simulates no hardware traffic."},
    {"Protocol1.UART.writeText", "writeText(string) : {ok,...}", "UTF-8 text", "UTF-8", "Protocol1.UART.writeText('STATUS\\n');", "Converts to UTF-8; for arbitrary binary data use write()."},
    {"Protocol1.UART.read", "read(maxBytes=4096) : {ok,data,count,parityErrorIndex,overflow}", "1..65536 max bytes", "bytes", "var r=Protocol1.UART.read(1024); print(r.data,r.parityErrorIndex);", "Manual mode reads the DWF SDK directly (unchanged). Automatic mode drains the native bounded queue without a second SDK reader. Queued data survives stopAutoRx until read or Protocol1.stop()."},
    {"Protocol1.UART.startAutoRx", "startAutoRx({pollIntervalMs:10,bufferSize:65536}) : boolean", "2..1000 ms, queue 1024..1048576 bytes", "ms; bytes", "if(!Protocol1.UART.startAutoRx({pollIntervalMs:10,bufferSize:65536})) throw Error(Protocol1.UART.lastError);", "Starts native QTimer polling on PanelRuntime's QJSEngine worker thread. Drains the DWF receiver into a bounded queue; does not depend on JavaScript timers. Call configure() first. Cannot run during blocking JavaScript."},
    {"Protocol1.UART.stopAutoRx", "stopAutoRx() : boolean", "No arguments", "", "Protocol1.UART.stopAutoRx();", "Stops the Qt timer but retains SDK UART configuration. Protocol1.stop() releases digital resources. After stopping, read() drains any queued bytes before direct SDK reads."},
    {"Protocol1.UART.onData", "onData(callback) : boolean", "function({data,text,count,overflow,parityErrorIndex,timestamp,timestampSource})", "bytes", "Protocol1.UART.onData(function(chunk){print(chunk.text);});", "Registers one optional JS callback, invoked on the QJSEngine thread after a native poll. Receives a copy, never consumes the read() queue. Timestamp is host-poll time, not bit-edge time. Callback exceptions use Panel diagnostics."},
    {"Protocol1.UART.clearOnData", "clearOnData() : boolean", "No arguments", "", "Protocol1.UART.clearOnData();", "Removes JS receive callback. Native UART Terminal display and receive queue continue independently."},
    {"Protocol1.UART.available", "available() : integer", "No arguments", "bytes", "print(Protocol1.UART.available());", "Number of unread bytes in WFScript's native queue. Returns 0 when in manual SDK mode or no queued data."},
    {"Protocol1.UART.stats", "stats() : {autoReceiving,available,receivedBytes,readBytes,bufferCapacity,queueDroppedBytes,sdkOverflows,parityErrors,...}", "No arguments", "counts", "print(JSON.stringify(Protocol1.UART.stats()));", "Cumulative receive statistics for the configured session. SDK overflow and WFScript application-queue drops are counted separately."},
    {"Protocol1.UART.autoReceiving", "autoReceiving : boolean", "Read-only", "", "print(Protocol1.UART.autoReceiving);", "Native UART receiver poll timer running."},
    {"Protocol1.UART.Receiver", "Receiver() : boolean", "No arguments", "", "Protocol1.UART.Receiver();", "WaveForms-style alias for startRx()."},
    {"Protocol1.UART.Receive", "Receive() : string", "No arguments", "UTF-8", "print(Protocol1.UART.Receive());", "WaveForms-style text receive; reads up to 4096 bytes and decodes as UTF-8."},
    {"Protocol1.UART.ReceiveArray", "ReceiveArray() : number[]", "No arguments", "bytes", "print(Protocol1.UART.ReceiveArray());", "WaveForms-style binary receive alias. Maximum 4096 bytes per call."},
    {"Protocol1.UART.Send", "Send(text,appendLF=false) : boolean", "Text and optional LF append", "UTF-8", "Protocol1.UART.Send('PING',true);", "WaveForms-style alias. Only LF ending supported here; full application line-ending GUI not implemented."},
    {"Protocol1.UART.SendArray", "SendArray(bytes,appendLF=false) : boolean", "Byte array and optional LF", "bytes", "Protocol1.UART.SendArray([0x01,0x02]);", "WaveForms-style send with optional LF."},
    {"Protocol1.SPI.configure", "configure({clock,mosi,miso,select,frequencyHz,mode,msbFirst,...}) : boolean", "Four distinct DIO 0..15; optional dq2/dq3, duty and delays", "Hz", "Protocol1.SPI.configure({clock:0,mosi:1,miso:2,select:3,frequencyHz:1000000,mode:0});", "Native master only. Supports 1/2/4 data lanes; select toggling may be host-controlled, not cycle-exact. Passive SPI Spy and slave emulation require separate implementations."},
    {"Protocol1.SPI.transfer", "transfer(tx,readCount=tx.length,bits=8,lanes=1) : {ok,rx,words}", "Integer word array; count; 1..32 bits; lane count 0/1/2/4", "words", "var r=Protocol1.SPI.transfer([0x9F,0,0,0]); print(r.rx);", "For large numbers split transfers or use command(); support bounded 65536 words. Simulation returns marked synthetic zeros."},
    {"Protocol1.SPI.command", "command(cmdBits,cmdValue,dummyBits,tx,rxCount,bits=8,lanes=1) : object", "Command 1..8 bits, 0..1024 dummy cycles; data words", "bits", "var r=Protocol1.SPI.command(8,0x9F,0,[],3,8,1); print(r.rx);", "FDwfDigitalSpiCmdWriteRead family; command/dummy native SDK operation."},
    {"Protocol1.SPI.Start", "Start() : boolean", "No arguments", "", "Protocol1.SPI.Start();", "WaveForms-style manual chip select assert. Call Stop afterward."},
    {"Protocol1.SPI.Stop", "Stop() : boolean", "No arguments", "", "Protocol1.SPI.Stop();", "Deasserts chip select but leaves protocol configured. Protocol1.stop() releases engines."},
    {"Protocol1.SPI.ReadWrite", "ReadWrite(bits,wordArray) : number[]", "1..32 bits per word; TX words", "words", "var id=Protocol1.SPI.ReadWrite(8,[0x9F,0,0,0]);", "WaveForms-style SPI master transfer alias, 4-wire."},
    {"Protocol1.SPI.Write", "Write(bits,wordArray) : boolean", "1..32 bits per word", "", "Protocol1.SPI.Write(8,[0x00,0x01]);", "Standard MOSI write."},
    {"Protocol1.SPI.Read", "Read(bits,count) : number[]", "1..32 bits; word count", "", "var x=Protocol1.SPI.Read(8,4);", "Reads standard MISO."},
    {"Protocol1.SPI.Read0", "Read0(bits,count) : number[]", "1..32 bits; word count", "", "var x=Protocol1.SPI.Read0(8,4);", "Reads 3-wire DQ0/SISO."},
    {"Protocol1.SPI.WriteDual", "WriteDual(bits,words) : boolean", "Data on DQ0/1", "", "Protocol1.SPI.WriteDual(8,[0xAB]);", "Dual output; ensure DQ lines connected."},
    {"Protocol1.SPI.ReadDual", "ReadDual(bits,count) : number[]", "Data on DQ0/1", "", "var x=Protocol1.SPI.ReadDual(8,4);", "Dual input."},
    {"Protocol1.SPI.WriteQuad", "WriteQuad(bits,words) : boolean", "Data on DQ0..3", "", "Protocol1.SPI.WriteQuad(8,[0xAB]);", "Quad output; configure DQ2/DQ3."},
    {"Protocol1.SPI.ReadQuad", "ReadQuad(bits,count) : number[]", "Data on DQ0..3", "", "var x=Protocol1.SPI.ReadQuad(8,4);", "Quad input; configure DQ2/DQ3."},
    {"Protocol1.I2C.configure", "configure({scl,sda,rateHz,timeoutSeconds,clockStretching}) : boolean", "Distinct DIO, default 100kHz", "Hz", "Protocol1.I2C.configure({scl:0,sda:1,rateHz:100000});", "Requires external pull-ups; bus master. 7-bit script addresses converted to 8-bit DWF addressing."},
    {"Protocol1.I2C.writeRead", "writeRead(address7,tx,readCount) : {ok,data,nackIndex,acknowledged}", "7-bit address 0..127; bytes; read count", "bytes", "var r=Protocol1.I2C.writeRead(0x48,[0x00],2); if(!r.acknowledged) throw Error('I2C NACK '+r.nackIndex);", "Native repeated-start. NACK index zero means ACK; negative means communication failure; caller must examine acknowledged."},
    {"Protocol1.I2C.write", "write(address7,bytes) : object", "7-bit address, bytes", "", "var r=Protocol1.I2C.write(0x48,[0x00,0x01]);", "Native write-only transfer; inspect nackIndex."},
    {"Protocol1.I2C.read", "read(address7,count) : object", "7-bit address, count", "", "var r=Protocol1.I2C.read(0x48,2);", "Native read-only transfer; inspect nackIndex."},
    {"Protocol1.I2C.Clear", "Clear() : boolean", "No arguments", "", "print(Protocol1.I2C.Clear());", "Attempts bus recovery; returns false when bus remains busy."},
    {"Protocol1.I2C.Receiver", "Receiver() : boolean", "No arguments", "", "Protocol1.I2C.Receiver();", "Starts native FDwfDigitalI2cSpyStart. No timestamp precision guarantee."},
    {"Protocol1.I2C.spyPoll", "spyPoll(maxBytes=256) : {ok,start,stop,data,nackIndex,warning,timestampSource}", "1..65536 words", "bytes", "var t=Protocol1.I2C.spyPoll(256); print(t.data,t.nackIndex);", "SDK decoded I2C frames. start=1 start, 2 restart; negative NACK values signal decoder errors/overflow. Host polling timestamp is not bus edge timestamp."},
    {"Protocol1.I2C.Receive", "Receive() : number[]", "No arguments", "bytes", "print(Protocol1.I2C.Receive());", "Partial WaveForms-style markers -1 START, -2 RESTART, -3 STOP, followed by SDK raw bytes; NOT WaveForms 9-bit data/ACK words. Use spyPoll() for separate NACK index."},
    {"Protocol1.I2C.Write", "Write(address7,bytes) : number", "7-bit address, byte array", "", "var nack=Protocol1.I2C.Write(0x48,[0x00]);", "WaveForms-style status 0 success, nonzero NACK, -2 SDK failure."},
    {"Protocol1.I2C.Read", "Read(address7,count) : number[]", "7-bit address, number of bytes", "", "var bytes=Protocol1.I2C.Read(0x48,2);", "WaveForms-style read alias. Empty on SDK failure; use lower-case read for detailed errors."},
    {"Protocol1.I2C.ReadSub", "ReadSub(address7,subaddress,count) : number[]", "7-bit address, subaddress bytes, count", "", "var x=Protocol1.I2C.ReadSub(0x48,[0x00],2);", "Repeated-start read; matches WaveForms 3-argument Read overload with explicit function name."},
    {"Protocol1.CAN.configure", "configure({tx,rx,rateHz,inverted}) : boolean", "Two DIO pins; classical CAN bit rate", "Hz", "Protocol1.CAN.configure({tx:0,rx:1,rateHz:500000});", "Electrical CAN transceiver with termination is required. AD3 DIO is NOT direct CANH/CANL. No CAN FD support."},
    {"Protocol1.CAN.send", "send(id,extended,remote,byteArray) : object", "Standard 11-bit or extended 29-bit identifier; DLC <=8", "bytes", "var r=Protocol1.CAN.send(0x123,false,false,[1,2,3]);", "Classical CAN only. SDK Tx() is synchronous; no bus timestamp."},
    {"Protocol1.CAN.receive", "receive() : {ok,received,id,extended,remote,dlc,status,data}", "Call startRx() first", "bytes", "var frame=Protocol1.CAN.receive(); if(frame.received) print(frame.id,frame.data);", "Status: 0 none, 1 received, 2 stuffing error, 3 CRC error. HostPoll timestamp only."},
    {"Protocol1.CAN.startRx", "startRx() : boolean", "No arguments", "", "Protocol1.CAN.startRx();", "Initialize SDK CAN receiver."},
    {"Protocol1.CAN.Send", "Send(BI,IDE,IE,RTR,DLC,bytes) : boolean", "Base ID, extension, remote, length and bytes", "", "Protocol1.CAN.Send(0x123,0,0,0,2,[1,2]);", "WaveForms-style basic classical CAN, not CAN FD."},
    {"Protocol1.CAN.Receiver", "Receiver() : boolean", "No arguments", "", "Protocol1.CAN.Receiver();", "WaveForms-style alias for startRx."},
    {"Protocol1.CAN.Receive", "Receive() : number[]", "No arguments", "", "var frame=Protocol1.CAN.Receive();", "Reduced WaveForms-like frame array: CRC and ACK are unavailable from SDK, so null is returned for those positions."},
    {"Protocol1.SWD.configure", "configure({clock,data,rateHz,turnaround}) : boolean", "DIO clock/data pins and rate", "Hz", "Protocol1.SWD.configure({clock:0,data:1,rateHz:1000000});", "ARM SWD host operations; verify target voltage and IO levels."},
    {"Protocol1.SWD.clear", "clear(resetClocks=52,trailClocks=16) : boolean", "SWD line reset length and idle clocks", "cycles", "Protocol1.SWD.clear();", "Native SWD reset sequence."},
    {"Protocol1.SWD.readRegister", "readRegister(accessPort,addressIndex) : {ok,ack,ackName,value,parityError}", "AP vs DP flag, address A3:A2 index 0..3", "32-bit", "var r=Protocol1.SWD.readRegister(false,0); print(r.ackName,r.value);", "ACK: 1 OK,2 WAIT,4 FAULT. Repeat only on WAIT; check parity."},
    {"Protocol1.SWD.writeRegister", "writeRegister(accessPort,addressIndex,uint32Value) : {ok,ack,ackName}", "AP/DP, address index and uint32", "32-bit", "var r=Protocol1.SWD.writeRegister(true,0,0x12345678);", "Native SWD write. No full SWD/JTAG debugger implemented."},
    {"Patterns1.runSequence", "runSequence(pins, bitStrings, bitRateHz) : boolean", "1..3 distinct DIO pins; 2..1024 bits/channel; 10..1,000,000 logical bits/s", "bits/s", "if(!Patterns1.runSequence([0,1,2],[\"01010101\",\"00110011\",\"00001111\"],1000)) throw Error(Patterns1.lastError);", "WFScript-specific extension, not a native WaveForms script method. The first character in each string is generated first, all strings must contain only 0/1 and be equal length. One shared DigitalOut start and divider keep channels aligned. Repeats continuously until stop(); SDK range and buffer limits are discovered at runtime. All output pins return to high-Z on stop. For tests connect DIO0->DIO3, DIO1->DIO4, DIO2->DIO5 through separate 1k resistors."},
    {"Patterns1.configSequence", "configSequence(pins, bitStrings, bitRateHz) : boolean", "Same arguments as runSequence", "bits/s", "Patterns1.configSequence([0,1],[\"0101\",\"0011\"],1000);", "Configure repeating multi-pin custom DigitalOut buffers but do not start them. Call runSequence to start. Stop releases owned outputs to high impedance."},
    {"Patterns1.runEncoder", "runEncoder(aPin, bPin, indexPin, ppr, rpm, reverse) : boolean", "Three distinct DIO pins 0..15; 1..1024 PPR; 1..10000 RPM; reverse boolean", "PPR, RPM", "if(!Patterns1.runEncoder(0,1,2,16,600,false)) throw Error(Patterns1.lastError);", "WFScript-specific convenience extension: generates coherent Gray-code quadrature A/B and one index pulse per revolution using one common DigitalOut start. Hardware custom buffer must hold 4*PPR bits on ALL three channels. Direction changes require stop/restart. Call stop() to release all three outputs to Z. Use a 3-resistor DIO0->3, 1->4, 2->5 loopback for physical verification."},
    {"Patterns1.configEncoder", "configEncoder(aPin, bPin, indexPin, ppr, rpm, reverse) : boolean", "Same arguments as runEncoder", "PPR, RPM", "Patterns1.configEncoder(0,1,2,16,600,false);", "Load synchronized encoder data without starting the output. Explicit runEncoder() starts. Not a native WaveForms scripting method: WFScript extension."},
    {"Patterns1.Channels.DIO1.DIO.value", "DIO.value : integer", "Physical output pin, 0..15", "pin", "print(Patterns1.Channels.DIO1.DIO.value);", "Second logical channel for an encoder B phase; last configured physical pin readback."},
    {"Patterns1.Channels.DIO2.DIO.value", "DIO.value : integer", "Physical output pin, 0..15", "pin", "print(Patterns1.Channels.DIO2.DIO.value);", "Third logical channel for index output; last configured physical pin readback."},
    {"Patterns1.run", "run() : boolean", "No arguments", "", "Patterns1.Channels.DIO0.Frequency.value=1000;\nif(!Patterns1.run()) throw Error(Patterns1.lastError);", "Starts one native DigitalOut Clock pattern. Explicit start only; no output when the Panel loads. Return output to high-Z with stop()."},
    {"Patterns1.config", "config() : boolean", "No arguments", "", "if(!Patterns1.config()) throw Error(Patterns1.lastError);", "Stages and commits DigitalOut without starting generation. Output idle state is Z."},
    {"Patterns1.stop", "stop() : boolean", "No arguments", "", "if(!Patterns1.stop()) print(Patterns1.lastError);", "Stops the pattern generator and disables the owned channel output. The runtime attempts stop at script/Panel exit."},
    {"Patterns1.status", "status() : object", "No arguments", "Hz and percent", "var p=Patterns1.status(); print(p.actualFrequencyHz,p.actualDutyPercent,p.simulation);", "Returns cached running/mode plus the SDK-read sdkStateName, sdkStateCode, sdkRunning, sdkRepeatCount (0=infinite), sdkStatusAvailable, sdkRepeatAvailable and achieved encoder RPM or sequence bit rate. Sequence status includes sequencePins, sequenceBitStrings, sequenceLength, sequenceActualBitRateHz and sequenceRepeatFactor. sdkRepeatCount is diagnostic only and may be nonzero despite continuous Run=0. Native fields are optional and distinguish actual DigitalOut from the cached workflow flag."},
    {"Patterns1.Channels.DIO0.DIO.value", "DIO.value : integer", "Physical AD3 pin index 0..15", "pin", "Patterns1.Channels.DIO0.DIO.value=0;", "WaveForms-compatible physical pin selector. Synonym for WFScript Pin.value; one clock/PWM channel in this revision."},
    {"Patterns1.Channels.DIO0.Pin.value", "Pin.value : integer", "Integer pin index 0..15", "pin", "Patterns1.Channels.DIO0.Pin.value=0;", "One logical WaveForms-style Pattern channel selects a physical pin. For custom 1..3 channel bit sequences use Patterns1.runSequence()."},
    {"Patterns1.Channels.DIO0.Type.text", "Type.text : string", "Clock", "", "Patterns1.Channels.DIO0.Type.text='Clock';", "Clock uses run(); Custom is readback after runEncoder() or runSequence(). The array-based multi-channel sequencer is a WFScript extension."},
    {"Patterns1.Channels.DIO0.Output.text", "Output.text : string", "PP", "", "Patterns1.Channels.DIO0.Output.text='PP';", "Push-pull only; requires 3.3-V-safe hardware and no external drivers on the selected pin."},
    {"Patterns1.Channels.DIO0.Idle.text", "Idle.text : string", "Z", "", "Patterns1.Channels.DIO0.Idle.text='Z';", "High-impedance idle state. Explicit stop also disables the Pattern channel output."},
    {"Patterns1.Channels.DIO0.Frequency.value", "Frequency.value : number", "1 to 1,000,000", "Hz", "Patterns1.Channels.DIO0.Frequency.value=1000;", "Clock frequency. Achieved frequency is returned by Frequency.real after configuring."},
    {"Patterns1.Channels.DIO0.Frequency.real", "Frequency.real : number", "Read only", "Hz", "print(Patterns1.Channels.DIO0.Frequency.real);", "Achieved DigitalOut counter/divider frequency from the SDK-reported base clock, not a measured signal."},
    {"Patterns1.Channels.DIO0.Duty.value", "Duty.value : number", "1 to 99", "%", "Patterns1.Channels.DIO0.Duty.value=50;", "Duty percentage; achieved value may differ after integer counter rounding."},
    {"Patterns1.Channels.DIO0.Duty.real", "Duty.real : number", "Read only", "%", "print(Patterns1.Channels.DIO0.Duty.real);", "Achieved DigitalOut counter setting; physical duty is measured separately through Logic1."},
    {"Logic1.single", "single() : boolean", "No arguments", "", "Logic1.Time.Rate.value=20000;\nLogic1.Time.Samples.value=4096;\nif(!Logic1.single()) throw Error(Logic1.lastError);", "Starts an untriggered 16-bit acquisition. This function returns after arming; call wait() to finish and transfer samples. No digital output is driven."},
    {"Logic1.wait", "wait(timeoutSeconds = 5) : boolean", "Positive timeout seconds", "s", "if(!Logic1.wait(3)) throw Error(Logic1.lastError);", "Waits until acquisition Done and loads the 16-bit sample words. Returns false on timeout or stop."},
    {"Logic1.captureDiagnostics", "captureDiagnostics() : object", "No arguments; last completed capture", "words / bytes / transitions", "var d=Logic1.captureDiagnostics();\nprint(d.bytesRequested, d.firstHalfChanges, d.secondHalfChanges);", "WFScript extension: reports requested/returned 16-bit word count, requested BYTES, optional SDK valid-sample count and state, and first/second-half DIO0..5 activity. Zero activity is valid for an idle input. The SDK data API accepts bytes, not sample count; this is fixed in 5.6.49.2."},
    {"Logic1.samples", "samples() : integer[]", "No arguments", "16-bit masks", "var data=Logic1.samples();\nvar dio1=data.map(w=>(w>>1)&1);", "Returns complete sample words after a successful wait(). Each bit corresponds to one DIO pin (0..15)."},
    {"Logic1.Time.Rate.value", "Rate.value : number", "Sample rate in Hz", "Hz", "Logic1.Time.Rate.value=20000;", "SDK reads the native DigitalIn clock and rounds to a positive integer divider. Rate.real and Logic1.sampleRateHz show effective sampling rate after single()."},
    {"Logic1.Time.Rate.real", "Rate.real : number", "Read only", "Hz", "print(Logic1.Time.Rate.real);", "Effective DigitalIn sample frequency from device clock divided by configured integer divider."},
    {"Logic1.Time.Samples.value", "Samples.value : number", "16..32768 samples, subject to device limit", "samples", "Logic1.Time.Samples.value=4096;", "v5.6.47 bounded single capture; requests larger than hardware DigitalIn buffer are rejected."},
    {"Logic1.sampleRateHz", "sampleRateHz : number", "Read only", "Hz", "print(Logic1.sampleRateHz);", "Actual digital acquisition sample rate after Logic1.single(). Use this value for plot timebase."},

    {"Device.isConnected", "isConnected() : boolean", "No arguments", "", "if (!Device.isConnected()) throw new Error(\"No device\");", "A connected AD3 or the selected simulation can satisfy this query."},
    {"StaticIO.onChange", "onChange(pin, callback, {edge:'change',debounceMs:30}) : boolean", "pin 0..15, callback(value,pin,event), edge change/rising/falling, debounce 0..1000 ms", "",
     "if(!StaticIO.onChange(0,function(level,pin,event){ print('DIO'+pin,level,event.edge); },{edge:'falling',debounceMs:30})) throw Error(StaticIO.lastError);\nif(!StaticIO.startMonitor({pollIntervalMs:20})) throw Error(StaticIO.lastError);",
     "Registers or replaces one callback on a digital input WITHOUT starting polling. Host-observed transitions after stable debounce only. Event timestamps are HOST-poll times, not hardware-captured edge times. Panel scripts only."},
    {"StaticIO.startMonitor", "startMonitor({pollIntervalMs:20}) : boolean", "pollIntervalMs integer 10..1000, default 20", "ms",
     "StaticIO.onChange(0,function(v){print('START',v);},{edge:'falling',debounceMs:30});\nif(!StaticIO.startMonitor({pollIntervalMs:20})) throw Error(StaticIO.lastError);",
     "Opt-in only: obtains an initial snapshot, then uses one Qt timer and one read-only DigitalIOStatus/InputStatus snapshot per poll for all 16 pins. Does not drive outputs, reset instruments or alter Scope/Wavegen configuration. SDK/USB contention and JS delays remain possible."},
    {"StaticIO.stopMonitor", "stopMonitor() : void", "No arguments", "",
     "StaticIO.stopMonitor();",
     "Stops Qt polling immediately without modifying outputs. Registered callbacks stay available for an explicit later startMonitor(). clearOnChange/clearAllOnChange unregister them."},
    {"StaticIO.clearOnChange", "clearOnChange(pin) : void", "pin 0..15", "", "StaticIO.clearOnChange(0);",
     "Unregister the callback for one pin. Does not automatically stop the monitor."},
    {"StaticIO.clearAllOnChange", "clearAllOnChange() : void", "No arguments", "", "StaticIO.clearAllOnChange();",
     "Remove all input-change callbacks. Polling remains active until stopMonitor() is explicitly called."},
    {"StaticIO.monitorStats", "monitorStats() : object", "No arguments", "polls, callbacks, changes, lastPollDurationUs",
     "var s=StaticIO.monitorStats();\nprint(s.monitoring,s.polls,s.lastPollDurationUs,s.watchCount);",
     "Host-side polling and callback diagnostics. Polling is NOT a real-time hardware edge detector; pulses shorter than the poll interval may be missed. An SDK error stops the monitor and never fabricates an input event."},
    {"StaticIO.monitoring", "monitoring : boolean", "Read-only", "true/false", "print(StaticIO.monitoring);",
     "True only while the opt-in input monitor's native Qt timer is running. Monitoring is off by default and stopped on Panel shutdown."},
    {"StaticIO.config", "config() : boolean", "No arguments", "", "StaticIO.Channel0.DIO0.value=1;\nif(!StaticIO.config()) throw Error(StaticIO.lastError);", "WFScript explicitly commits staged pin values; unlike the WaveForms Static I/O GUI, property assignment alone does not drive the pin. Disabled pins default to Z."},
    {"StaticIO.setPin", "setPin(index, state) : boolean", "index 0..15, state 0 low / 1 high / 2 high-Z", "", "if(!StaticIO.setPin(0,1)) throw Error(StaticIO.lastError);", "WFScript convenience method that updates and commits one pin; changes hardware outputs in Physical Device mode."},
    {"StaticIO.readPin", "readPin(index) : integer", "index 0..15", "0/1/-1", "print(StaticIO.readPin(1));", "Performs a new DigitalIOStatus snapshot before reading the input bit. -1 means failure; check StaticIO.lastError."},
    {"StaticIO.readGroup", "readGroup(start) : integer", "start=0 or 8", "0..255/-1", "print(StaticIO.readGroup(0));", "Reads all 8 pins in the group in one snapshot; -1 means an SDK error."},
    {"StaticIO.status", "status() : object", "No arguments", "16-bit masks", "var s=StaticIO.status();\nprint(s.valid, s.inputMask, s.outputEnableMask);", "Read-only DigitalIO snapshot. inputMask is actual hardware digital levels; a simulation returns ideal output echo with undriven inputs LOW."},
    {"StaticIO.capabilities", "capabilities() : object", "No arguments", "16-bit masks", "var c=StaticIO.capabilities();\nprint(c.outputMask.toString(16));", "Queries SDK capabilities without changing directions. 0xffff is the ideal simulation mask. availability is device-dependent."},
    {"StaticIO.stop", "stop() : boolean", "No arguments", "", "if(!StaticIO.stop()) print(StaticIO.lastError);", "Releases the StaticIO output-enable mask to 0 (all high impedance). Runtime performs cleanup if StaticIO was touched; failure to confirm high-Z is warned."},
    {"StaticIO.Channel0.DIO0.value", "value : integer", "0=LOW, 1=HIGH, 2=Z", "", "StaticIO.Channel0.DIO0.value=1;\nStaticIO.config();", "WaveForms-compatible property; this WFScript subset deliberately stages assignments until config(). DIO8..15 live under Channel1."},
    {"StaticIO.Channel0.DIO0.text", "text : string", "'0', '1', or 'Z'", "", "StaticIO.Channel0.DIO0.text='Z';\nStaticIO.config();", "Invalid text is rejected on commit. Staged output state is separate from physical input state."},
    {"StaticIO.Channel0.DIO0.Input.value", "value : integer", "Read-only", "0/1/-1", "print(StaticIO.Channel0.DIO0.Input.value);", "Fresh SDK snapshot of the pin's actual logic level. Returns -1 for SDK error, not a fabricated LOW."},
    {"StaticIO.Channel1.DIO8.value", "value : integer", "0=LOW, 1=HIGH, 2=Z", "", "StaticIO.Channel1.DIO8.value=2;\nStaticIO.config();", "Channel1 contains DIO8..15. Outputs default to high impedance."},
    {"StaticIO.Channel1.DIO8.Input.value", "value : integer", "Read-only", "0/1/-1", "print(StaticIO.Channel1.DIO8.Input.value);", "Fresh input level for DIO8. Use StaticIO.status() to read all pins efficiently."},
    {"StaticIO.Channel0.Input.value", "value : integer", "Read-only", "0..255/-1", "print(StaticIO.Channel0.Input.value);", "Eight-bit input mask for DIO0..7. SDK read error returns -1."},
    {"StaticIO.Channel1.Input.value", "value : integer", "Read-only", "0..255/-1", "print(StaticIO.Channel1.Input.value);", "Eight-bit input mask for DIO8..15. SDK read error returns -1."},
    {"Supplies.run", "run() : boolean", "No arguments", "", "Supplies.Output.PositiveSupply.Voltage.value = 3.3;\nSupplies.Output.PositiveSupply.Enable.value = 1;\nif (!Supplies.run()) throw Error(Supplies.lastError);", "Explicitly turns master on and commits both output channels. Power is OFF at Panel/script completion. No output is energized in simulation."},
    {"Supplies.config", "config() : boolean", "No arguments", "", "Supplies.MasterEnable.checked = true;\nif (!Supplies.config()) throw Error(Supplies.lastError);", "Commits the staged WaveForms-style settings; unlike WaveForms' UI-backed properties, WFScript requires an explicit commit. Invalid settings fail closed."},
    {"Supplies.stop", "stop() : boolean", "No arguments", "", "if (!Supplies.stop()) print(Supplies.lastError);", "Disables AD3 supply master; the script and Panel runtime also call stop on cleanup if supplies were used."},
    {"Supplies.status", "status() : object", "No arguments", "V", "var s = Supplies.status();\nprint(s.measuredAvailable, s.positiveVoltage, s.negativeVoltage);", "Reads AD3 AnalogIO status. measuredAvailable=false means no physical voltage readback; simulation values are ideal and not measurements."},
    {"Supplies.MasterEnable.checked", "checked : boolean", "true/false", "", "Supplies.MasterEnable.checked = true;\nSupplies.config();", "WaveForms-compatible master enable property. Staged until config(); run() also turns on master."},
    {"Supplies.Output.PositiveSupply.Enable.value", "value : integer", "0 or 1", "", "Supplies.Output.PositiveSupply.Enable.value = 1;", "V+ channel enabled only if master is also on. Changes require config()/run()."},
    {"Supplies.Output.NegativeSupply.Enable.value", "value : integer", "0 or 1", "", "Supplies.Output.NegativeSupply.Enable.value = 1;", "V- channel enabled only if master is also on. Changes require config()/run()."},
    {"Supplies.Output.PositiveSupply.Voltage.value", "value : number", "0.5 to 5.0", "V", "Supplies.Output.PositiveSupply.Voltage.value = 3.3;", "AD3 voltage command, bounded; changes are staged until config()/run()."},
    {"Supplies.Output.NegativeSupply.Voltage.value", "value : number", "-5.0 to -0.5", "V", "Supplies.Output.NegativeSupply.Voltage.value = -3.3;", "Negative rail signed voltage. Both setpoints must remain in range, even when output disabled."},
    {"Supplies.Output.PositiveSupply.Voltage.real", "real : number", "No arguments", "V", "Supplies.status(); print(Supplies.Output.PositiveSupply.Voltage.real);", "Most recent SDK voltage status, not guaranteed accurate if readback unavailable. Check status().measuredAvailable."},
    {"Supplies.Output.NegativeSupply.Voltage.real", "real : number", "No arguments", "V", "Supplies.status(); print(Supplies.Output.NegativeSupply.Voltage.real);", "Most recent SDK voltage status; check status().measuredAvailable. Simulation reports an ideal model."},
    {"Wavegen1.run", "run() : boolean", "No arguments", "", "if (!Wavegen.run()) throw new Error(Wavegen.lastError);", "Configures and starts enabled waveform generators. Changes physical outputs in Hardware mode."},
    {"Wavegen1.stop", "stop() : boolean", "No arguments", "", "Wavegen.stop();", "Stops the generator."},
    {"Wavegen1.Channel1.enable", "enable : boolean", "true or false", "", "Wavegen.Channel1.enable = true;", "Selects whether a generator channel is enabled."},
    {"Wavegen1.Channel1.Simple.Frequency.value", "Frequency.value : number", "Frequency in hertz", "Hz", "Wavegen.Channel1.Simple.Frequency.value = 1000;", "Effective output range depends on the device configuration."},
    {"Wavegen1.Channel1.Simple.Amplitude.value", "Amplitude.value : number", "Amplitude in volts (peak)", "V peak", "Wavegen.Channel1.Simple.Amplitude.value = 1;", "The generated sine wave has approximately 0.707 V RMS at 1 V peak."},
    {"Wavegen1.Channel1.Simple.Offset.value", "Offset.value : number", "DC offset", "V", "Wavegen.Channel1.Simple.Offset.value = 0;", "Output voltage limits depend on range and offset combined."},
    {"Wavegen1.Channel1.Simple.Type.text", "Type.text : string", "Supported waveform name", "", "Wavegen.Channel1.Simple.Type.text = \"Sine\";", "Use autocomplete inside quotes for available choices; selected hardware may restrict them."},
    {"Wavegen1.Channel1.Mode.text", "Mode.text : string", "Generator operating mode", "", "Wavegen.Channel1.Mode.text = \"Simple\";", "Use autocomplete inside quotes for supported modes."},
    {"Scope1.single", "single() : boolean", "No arguments", "", "if (!Scope.single()) throw new Error(Scope.lastError);", "Arms an acquisition. Call Scope.wait() to obtain completed data."},
    {"Scope1.wait", "wait(timeoutSeconds = 5) : boolean", "Optional timeout in seconds", "s", "if (!Scope.wait(5)) throw new Error(Scope.lastError);", "Checks acquisition completion. Do not assume data is ready before the wait succeeds."},
    {"Scope1.Time.Rate.value", "Rate.value : number", "Requested acquisition rate", "samples/s", "Scope.Time.Rate.value = 1000000;", "Actual supported rate depends on AD3 configuration."},
    {"Scope1.Time.Samples.value", "Samples.value : number", "Requested acquisition buffer size", "samples", "Scope.Time.Samples.value = 16384;", "For larger captures, use Scope Record mode if supported."},
    {"Scope1.Channel1.Range.value", "Range.value : number", "Requested oscilloscope range in peak-to-peak volts", "Vpp", "Scope1.Channel1.Range.value = 10;", "Existing WaveForms-style property. Hardware may round or map requested Vpp to an available gain/range. Call Scope1.config() or single() before checking Range.real."},
    {"Scope1.Channel1.Range.real", "Range.real : number", "Read-only effective range, following successful config/single", "Vpp", "Scope1.Channel1.Range.value = 10;\nif (!Scope1.config()) throw Error(Scope1.lastError);\nprint(Scope1.Channel1.Range.real);", "Reads FDwfAnalogInChannelRangeGet for actual hardware configuration. In simulation the readback equals the requested range. Do not interpret as an extra physical gain stage."},
    {"Scope1.Channel1.Range.steps", "Range.steps : number[]", "Read-only SDK-advertised selectable ranges", "Vpp", "var ranges = Scope1.Channel1.Range.steps;\nprint(ranges);", "Read from FDwfAnalogInChannelRangeSteps when the device is opened. [] means no reported values, including in simulation; see Scope1.capabilities().rangeStepsAvailable. These are selectable settings, not separate hardware amplifier gains."},
    {"Scope1.capabilities", "capabilities() : object", "No arguments", "Vpp and other units", "var c=Scope1.capabilities();\nprint(c.rangeStepsAvailable, c.rangeSteps, c.rangeMinVpp, c.rangeMaxVpp);", "Includes device-advertised rangeSteps, rangeStepsAvailable, rangeMinVpp, rangeMaxVpp and rangeStepCount where the SDK exposes them. Range discovery is non-mutating and optional."},
    {"Scope1.Channel1.measure", "measure(name [, startSeconds, endSeconds]) : number", "Measurement name, optional start/end seconds", "Depends on measurement", "var rms = Scope.Channel1.measure(\"RMS\");", "Computes from the last completed capture. Ctrl+Space after the opening quote suggests measurement names."},
    {"Scope1.Channel1.measureWindow", "measureWindow(name, startSeconds, endSeconds) : number", "Measurement name, start/end times (seconds)", "Depends on measurement", "var rms = Scope.Channel1.measureWindow(\"RMS\", 0, 0.05);", "Time bounds must be ordered and fit the capture."},
    {"Scope1.Channel1.measureCapture", "measureCapture(name [, startSeconds, endSeconds]) : number", "Measurement name, optional time bounds", "Depends on measurement", "var hz = Scope.Channel1.measureCapture(\"Frequency\");", "Uses captured sample data, not a live hardware query."},
    {"Scope1.Channel1.spectrum", "spectrum(fftSize, window) : object", "Power-of-two FFT size, window: Hann, Rectangular, Blackman", "Hz, V", "var fft = Scope.Channel1.spectrum(16384, \"Hann\");\nprint(fft.peakFrequencyHz);", "Returns numeric spectrum data. Frequency and amplitude depend on window and sampling conditions."},
    {"Scope1.Channel1.statistics", "statistics([startSeconds, endSeconds]) : object", "Optional start and end times", "mixed", "var stats = Scope.Channel1.statistics();\nprint(stats.rms, stats.min, stats.max);", "Computes measurements in native C++ from the last capture."},
    {"Scope1.captureInfo", "captureInfo() : object", "No arguments", "", "var info = Scope.captureInfo();\nprint(info.sampleCount, info.sampleRateHz);", "Host-after-transfer timestamp differs from the DWF trigger timestamp."},
    {"Scope1.saveReference", "saveReference(channelNumber) : boolean", "Channel index 1 or 2", "", "Scope.saveReference(1);", "Copies the last completed channel buffer; reference persists across captures."},
    {"Scope1.mathData", "mathData(operation) : array", "CH1-CH2, CH1+CH2, CH1*CH2, CH1-REF1, CH2-REF2", "Varies", "var diff = Scope.mathData(\"CH1-REF1\");", "Requires matching sample counts; reference math also requires matching sampling rates."},
    {"Scope1.Time.Mode.text", "Mode.text : string", "Repeated, Shift, Record, or hardware-supported mode", "", "Scope.Time.Mode.text = \"Record\";", "Some modes are unavailable on certain hardware; check Scope.capabilities()."},
    {"Wavegen1.Channel1.Modulation.FM.enable", "FM.enable : boolean", "true or false", "", "Wavegen.Channel1.Modulation.FM.enable = false;", "Enabling FM changes generator output; check supported hardware modes."},
    {"Wavegen1.play", "play(channel, timeoutSeconds) : boolean", "Channel 1/2, timeout seconds", "s", "if (!Wavegen.play(1, 2)) throw new Error(Wavegen.lastError);", "For finite streaming playback after filling the Play buffer."},
    {"include", "include(path) : void", "Safe relative .js library name", "", "include(\"TextTools.js\");", "Runs a classic script once per engine in global scope. Use modules.load() for native .mjs exports."},
    {"modules.load", "load(path) : module", "Safe relative .mjs library name", "", "var math = modules.load(\"SharedMath.mjs\");", "Uses Qt native ECMAScript modules; same engine, with cached exports. Embedded panel libraries take priority over shared files."},
    {"modules.userFolder", "userFolder() : string", "No arguments", "", "print(modules.userFolder());", "Shows the writable per-user library folder. Manage modules via Script > Manage Script Libraries."},
    {"print", "print(value, ...) : void", "One or more output values", "", "print(\"Voltage:\", Scope.Channel1.measure(\"RMS\"));", "Prints to the Script output console."},
    {"wait", "wait(seconds)", "Delay in seconds", "s", "wait(0.100);", "This is the script delay helper, not Scope.wait()."},
    {"TestRunner.configure", "configure(options) : boolean", "{title?,sequenceId?,tableId?,logId?,summaryId?,stopOnFailure?,totalTimeoutMs?}", "ms", "TestRunner.configure({sequenceId:'stationSequence',totalTimeoutMs:5000,stopOnFailure:true});", "Native C++ executive. totalTimeoutMs is a monotonic total-run deadline in milliseconds; 0 disables it. UI targets are optional. Timed execution uses one worker-thread Qt event loop, not a blocking wait."},
    {"TestRunner.loadDefinition", "loadDefinition(document, libraryExports) : boolean", "{schemaVersion:1,id,title,config,steps[]} plus imported .mjs exports", "", "const seq=modules.load('TestStationSequence.mjs');\nconst impl=modules.load('TestStationSteps.mjs');\nif(!TestRunner.loadDefinition(seq.definition,impl)) throw Error(TestRunner.lastError);", "Manual-mode only for projects without a project-owned definition. Project-managed tests load automatically; do not call from operator script. Transactional C++ loader validates every step and resolves its handler/handlers names against callable exports. Replaces current steps/config only when all entries pass. Manual TestRunner.add() remains supported. Setup/Main/Cleanup ordering remains native; no JS eval or second engine."},
    {"TestRunner.loadDefinitionFile", "loadDefinitionFile(filePath, libraryExports) : boolean", "Existing file ending .wftest.json (max 128 KiB), imported .mjs module", "", "const impl=modules.load('TestStationSteps.mjs');\nTestRunner.loadDefinitionFile('C:/Tests/ControllerFunctionalTest.wftest.json',impl);", "Read and validate a versioned standalone JSON sequence file. Only the data file is loaded from disk; functions must already be exported by a separately loaded module. No file writes and no test execution occur on import. Invalid files preserve the previous executable sequence."},
    {"TestRunner.saveDefinition", "saveDefinition(filePath) : boolean", "Existing parent folder, destination ending .wftest.json", "UTF-8 JSON", "TestRunner.saveDefinition('C:/Tests/ControllerCopy.wftest.json');", "Explicitly export the most recently imported declarative definition atomically using QSaveFile. Rejects active executions, unsupported filename extensions and sequences assembled with manual add() calls. Saves configuration and named handlers, never source code or runtime results."},
    {"TestRunner.definition", "definition() : object", "No arguments", "Versioned JSON-like sequence", "const doc=TestRunner.definition();\nprint(doc.id,doc.steps.length);", "Read-only snapshot of the imported data-only definition; empty for sequences assembled exclusively with TestRunner.add(). Does not contain callable functions. Changes to runtime options through configure() do not rewrite the source definition."},
    {"TestRunner.add", "add(definition, callbackOrHandlers?) : boolean", "{id,name,group?,type?,description?,mode?,intervalMs?,timeoutMs?,durationMs?}, function OR {start?,poll?,finish?}", "ms", "TestRunner.add({id:'settle',name:'Settle',mode:'delay',durationMs:150,timeoutMs:500},null);", "Manual-mode only when no native project testDefinition is loaded. For normal project tests use the Visual Editor. Numeric steps can use scalar limits {low,high}, or named measurements: {vpp:{name:'Amplitude',low:1.9,high:2.1,unit:'Vpp'}} when returning a JSON object/array. v5.6.59: group=setup/main/cleanup (default main); C++ always executes Cleanup after failed/error Main tests. type=test (default, successful PASS) or action (successful DONE). Modes: once calls the function once; delay waits durationMs without calling JS; callback passes {id,name,params,token,runId} to its function and awaits TestRunner.complete/fail; poll calls {poll} on a best-effort Qt timer (intervalMs >= 10) until it returns true (PASS) or false (FAIL). Poll return undefined/null = pending. {start} runs once; {finish}(status,elapsedMs) runs at termination, including normal abort/reset when the engine is available. Callbacks may come directly from modules.load('...mjs') exports or included .js global functions. Synchronous hardware/JS calls cannot be force-preempted by a Qt timer."},
    {"TestRunner.complete", "complete(token, result) : boolean", "Unique callback token; number, JSON object/array, or action/test boolean", "", "export function readSupply(step) {\n    meter.read(function(err, reading) {\n        if (!TestRunner.isActive(step.token)) return;\n        if (err) TestRunner.fail(step.token, String(err));\n        else TestRunner.complete(step.token, reading);\n    });\n}", "RC1.5.10: callback completion supports finite scalar measurements (backwards compatible), JSON objects and arrays. For a numeric step define measurements with paths such as vpp, metrics.rms, channels[0], temperatures[*]; the native engine applies each low/high rule, writes separate Measurement Table entries, and retains the result in the JSON report. Extra JSON fields are metadata only. Structured results are limited to 32 KiB, arrays evaluated with [*] to 32 elements, and the current 100,000-measurement per-run limit applies. Tokens reject stale/duplicate replies but do not cancel SCPI operations; verify output OFF in Cleanup."},
    {"TestRunner.fail", "fail(token, reason) : boolean", "Token for the active callback-mode step, error message string", "", "if (TestRunner.isActive(step.token)) TestRunner.fail(step.token, 'DMM overload');", "Reports a callback-mode step ERROR and follows native Cleanup policy. A stale token returns false and cannot alter the next step. Not a hardware emergency shutdown."},
    {"TestRunner.isActive", "isActive(token) : boolean", "Per-invocation token received as step.token", "", "if (!TestRunner.isActive(step.token)) return;", "True only for the current live callback step before completion/STOP/timeout. Use before sending additional SCPI commands from asynchronous continuations. All JavaScript runs on the Panel worker thread."},
    {"TestRunner.onFinished", "onFinished(callback) : boolean", "function(summary) or null", "", "TestRunner.onFinished(function(r){print(r.state,r.elapsedMs+'ms');});", "Native completion notification after the last step, failure, timeout, or abort. The callback runs on the Panel worker thread and may read summary()/results(), but must not start another run synchronously. Needed because timed run() returns before the final result."},
    {"TestRunner.run", "run() : boolean", "No arguments", "", "if(!TestRunner.run()) print(TestRunner.lastError);", "Starts/continues native scheduling. For all-once sequences it retains synchronous behavior; for delay/poll steps true means accepted/running, NOT final PASS. Inspect state() later or register onFinished(). Explicit callback-mode completion is supported; returned Promises are not automatically awaited. No nested event loop."},
    {"TestRunner.next", "next() : boolean", "No arguments", "", "TestRunner.next();", "Begins one step only. A poll/delay step remains RUNNING and completes on a Qt timer; next() is rejected while a step is active. Subsequent steps require another next() or run()."},
    {"TestRunner.measure", "measure(name,value,low,high,unit?,resultWidget?) : boolean", "Measurement name, finite value and inclusive bounds", "Depends on unit", "return TestRunner.measure('V+',3.301,3.25,3.35,'V','resultPositive');", "Native inclusive limit evaluation, bounded results and automatic updates to table, result card and event log. Only callable during an executing step callback; do not call repeatedly during every pending poll unless intentional."},
    {"TestRunner.reset", "reset() : boolean", "No arguments", "", "TestRunner.reset();", "Clears results and restores PENDING steps. Can cancel a pending timed step; its finish handler is called with aborted before results are cleared. Cannot reset from inside an active callback."},
    {"TestRunner.clear", "clear() : boolean", "No arguments", "", "TestRunner.clear();", "Discard registered steps as well as results, only while idle/not in a session."},
    {"TestRunner.abort", "abort() : boolean", "No arguments", "", "TestRunner.abort();", "Emergency abort between scheduled callbacks. The active step's finish handler runs when safe, but the sequence Cleanup group is skipped. Prefer terminate() for normal operator stops. Native Panel Stop interrupts the engine and cannot forcibly cancel a blocking driver call."},
    {"TestRunner.terminate", "terminate() : boolean", "No arguments", "", "TestRunner.terminate();", "Cooperative stop: after the active callback returns, skip remaining Main steps and run the Cleanup group; the overall state is aborted. This is the preferred operator Stop for hardware test stations. Different from abort(), which stops immediately and skips sequence-level Cleanup."},
    {"TestRunner.setRunInfo", "setRunInfo(details) : boolean", "Legacy/dynamic tests: up to 16 primitive fields. Project-defined Run Information uses the native UUT widget instead.", "", "TestRunner.setRunInfo({uutId:'SIM-UUT-001',stationId:'Demo',operatorId:'DEMO'});", "C++ owns a bounded snapshot of this run's UUT/operator/station metadata. Values must be primitive strings, booleans or finite numbers. Fields are included in summary().runInfo and report().runInfo, not persisted to .wfpanel. Cannot change during execution. reset()/clear() remove previous run metadata; start a new run with setRunInfo() again."},
    {"TestRunner.runInfo", "runInfo() : object", "No arguments", "Run metadata", "print(TestRunner.runInfo().uutId);", "Frozen native C++ snapshot of current or last execution; not changed when editing next UUT."},
    {"TestRunner.pendingRunInfo", "pendingRunInfo() : object", "No arguments", "Pending UUT inputs", "const pending=TestRunner.pendingRunInfo(); print(pending.uutId);", "Live pending operator values from the one project-owned model shared by all Run Information widgets. Includes hidden Pages; no active widget. Numeric entries remain text until validated at run start. Does not overwrite report().runInfo for an already completed execution."},
    {"TestRunner.setVariable", "setVariable(name,value) : boolean", "Name string and primitive value", "", "TestRunner.setVariable('lastVoltage',3.301);", "Share a bounded primitive value between imported library step callbacks within the active C++ execution. Only valid inside a running step handler. Values are cleared on each run and reset; no JavaScript objects, functions or pointers are stored."},
    {"TestRunner.variable", "variable(name) : value", "Shared variable name", "", "const value=TestRunner.variable('lastVoltage');", "Returns the native value recorded by setVariable() or undefined/invalid if absent; use for cross-step communication without module globals."},
    {"TestRunner.variables", "variables() : object", "No arguments", "Shared variable snapshot", "print(JSON.stringify(TestRunner.variables()));", "Returns all named C++-owned values shared between test steps in this execution. Included in report().variables."},
    {"TestRunner.saveReport", "saveReport(filePath, format='json') : boolean", "Absolute or relative existing folder and .json/.csv file name; format json or csv", "UTF-8", "if (!TestRunner.saveReport('C:/TestReports/UUT-001.json')) print(TestRunner.lastError);", "Explicit export only after a completed run (PASS/FAIL/ERROR/ABORTED). JSON preserves the entire structured native report. CSV writes one STEP row per step and one MEASUREMENT row per measurement with linked stepId. Uses QSaveFile atomic replacement; parent directory must already exist. Does not run automatically on Panel load."},
    {"TestRunner.exportReport", "exportReport(format='json') : string", "Optional 'json' or 'csv'", "Path", "const path=TestRunner.exportReport('json'); if(path) print('Saved:',path); else print(TestRunner.lastError);", "Creates Documents/WFScript Reports when explicitly invoked for a terminal run. File name contains a sanitized UUT ID plus the unique execution ID; returns saved path or an empty string on error. JSON is the complete record; CSV is a tabular step/measurement export. Never writes on Panel load or while a test is still RUNNING."},
    {"TestRunner.archiveInfo", "archiveInfo() : object", "No arguments; native automatic report storage", "Status / path", "const a=TestRunner.archiveInfo(); print(a.status,a.path,a.error);", "Read-only status of the most recent automatic archive: idle, pending, disabled, saved or error. The project Test Sequence Editor > Report Storage owns auto-save and folder selection. A report is archived after all Cleanup finishes, even for failed/error/aborted executions. File failures do not change the test verdict. The onFinished handler can read archiveInfo(). No archive is written on Panel startup."},
    {"TestRunner.report", "report() : object", "No arguments", "JSON-compatible object", "const r=TestRunner.report(); print(JSON.stringify(r));", "C++ owned run record: schemaVersion, executionId, title, state, startedAtUtc, finishedAtUtc, elapsedMs, summary, steps[] (group,type,status,timing), measurements[] (stepId, value, limits, status), runInfo, and per-run variables. Automatic archiving is configured in Test Sequence > Report Storage; use archiveInfo() to get the result path. Manual saveReport() and exportReport() remain available. Also includes a definition SHA-256 and execution mode when available."},
    {"TestRunner.summary", "summary() : object", "No arguments", "counts and ms", "const s=TestRunner.summary();print(s.state,s.elapsedMs,s.stepElapsedMs,s.currentStepId);", "Native snapshot includes total and active step elapsed ms, pollCount, currentStepId, totalTimeoutMs, completion counts, failure counts and lastError. Timed steps remain RUNNING between callbacks."},
    {"TestRunner.results", "results() : array", "No arguments", "Varies", "const rows=TestRunner.results();", "Native bounded measurement records {name,value,low,high,unit,stepId,status}; updates the existing Measurement Table. Transient runtime state, not persisted to .wfpanel."},
    {"TestRunner.state", "state() : string", "No arguments", "", "print(TestRunner.state());", "idle/running/passed/failed/error/aborted. For timed tests, wait for onFinished or inspect the status after completion rather than reading it immediately after run()."},
    {"TestRunner.lastError", "lastError : string", "Read-only", "", "print(TestRunner.lastError);", "Native validation errors, timeout messages and JavaScript handler exceptions; original module filename/line retained in diagnostics."},
    {"ui.set", "set(widgetId, value) : boolean", "Panel widget ID and runtime value", "", "ui.set(\"resultPositive\",3.301);\nui.set(\"stationSequence\",[{id:\"power\",name:\"Check V+\",status:\"pending\"}]);\nui.set(\"stationSequence\",{update:{id:\"power\",status:\"pass\",elapsedMs:125,message:\"3.301 V\"}});\nui.set(\"stationLog\",{append:{level:\"PASS\",message:\"Power verified\",source:\"Station\"}});", "Measurement Result: numeric ui.set evaluates configured min/max inclusive, {reset:true} clears. Measurement Table: full array of {name,value,unit,low,high}, {append:oneMeasurement}, {append:[measurements...]}, or {reset:true}; native TestRunner normally streams these updates. Supports 100,000 results. Event Log: {append:{level,message,source?,timestamp?}}, {append:[entries...]}, full array to replace, or {clear:true}. Levels: DEBUG,INFO,PASS,WARN,FAIL,ERROR. GUI filter/Copy/Clear/Export CSV; timestamps default to local time; maxEntries bounds history, live entries not saved. Test Sequence: array of unique {id,name,description?,status?,message?,elapsedMs?} (maxSteps property), {update:{id,status?,message?,elapsedMs?,name?,description?}}, {reset:true} (retain steps, reset states), or {clear:true}. States: pending,running,pass,done,fail,error,skipped,aborted. The Test Sequence is a VIEW; the native TestRunner (if used) owns scheduling, statuses, deadlines and results. Direct ui.set() can still update a standalone sequence widget without TestRunner. Invalid updates are ignored atomically and logged to Qt warnings. ui.set confirms a known widget ID, not per-widget payload validation. Conversion from JS objects to native Qt values occurs on the script thread. Hidden Pages are updated; runtime values are NOT persisted to .wfpanel. See Examples > Getting Started > Automated Test Station."},
    {"ui.tableRowCount", "tableRowCount(tableId) : number", "Experiment DataTable widget ID", "rows", "var n=ui.tableRowCount('experimentTable');", "O(1) retained-row count for virtual Experiment DataTables; -1 for other widget IDs. Use instead of ui.get(id).length in high-rate callbacks."},
    {"ui.tableSlice", "tableSlice(tableId,start,count) : array|null", "Experiment DataTable ID; zero-based start; maximum count 2048", "rows", "var latest=ui.tableSlice('experimentTable',Math.max(0,ui.tableRowCount('experimentTable')-20),20);", "Returns a bounded nested Array snapshot, not a live reference. Indexes are relative to current rolling history. null for invalid requests."},
    {"ui.tableRow", "tableRow(widgetId,row) : array|null", "Experiment DataTable row index (zero-based)", "", "var record=ui.tableRow('experimentTable',0);", "Returns one retained row without a full-table snapshot; null for an invalid row."},
    {"ui.tableSelection", "tableSelection(widgetId) : object|null", "Experiment DataTable widget ID", "", "var selection=ui.tableSelection('experimentTable');", "Cached selection: {row,column,rows,selectedCells,truncated}. Selected rows are capped at 128; updated by selectionChanged events."},
    {"ui.tableSetTitle", "tableSetTitle(widgetId,title) : boolean", "Experiment DataTable widget ID, title", "", "ui.tableSetTitle('experimentTable','Live ADC readings');", "Changes the runtime title without changing the saved Panel definition."},
    {"ui.tableSetStatus", "tableSetStatus(widgetId,status) : boolean", "Experiment DataTable widget ID, status", "", "ui.tableSetStatus('experimentTable','Receiving UDP packets');", "Changes the short runtime status line."},
    {"ui.tableFollow", "tableFollow(widgetId,enabled) : boolean", "Experiment DataTable ID, follow boolean", "", "ui.tableFollow('experimentTable',false);", "Changes whether the view follows the bottom while receiving data. Browsing older rows remains anchored."},
    {"ui.tableImportCsv", "tableImportCsv(widgetId,path?) : boolean", "Experiment DataTable ID, optional path", "", "ui.tableImportCsv('experimentTable');", "Requests operator CSV import dialog, or imports provided path on GUI thread. Completion reported by importCompleted({ok,path,rows,error}). Returns only request acceptance."},
    {"ui.tableExportCsv", "tableExportCsv(widgetId,path?) : boolean", "Experiment DataTable ID, optional path", "", "ui.tableExportCsv('experimentTable');", "Requests operator CSV export dialog, or exports provided path on GUI thread. Completion reported by exportCompleted({ok,path,rows,error}). Returns only request acceptance."},
    {"ui.tableCopySelection", "tableCopySelection(widgetId) : boolean", "Experiment DataTable ID", "", "ui.tableCopySelection('experimentTable');", "Copies selected cells as tab-delimited text. Use CSV export for full datasets."},
    {"ui.get", "get(widgetId) : value", "Panel widget ID", "", "var setting = ui.get(\"speed\");", "Returns standard JavaScript primitives, arrays and plain objects (including true nested Array rows for Experiment Data Tables). No Qt QVariantList conversion is necessary: Array.isArray(ui.get(\"recipeTable\")) is true. A returned array/object is a snapshot; call ui.set() to commit changes. Experiment DataTable ui.get() returns its full current retained snapshot; for high-speed updates use ui.set(id,{appendRows:[...]}) and use ui.tableRowCount()/ui.tableSlice() for bounded reads. Reads the most recently assigned value from the persistent Panel runtime, even when the widget is on a hidden Page. Test Sequence initially returns its configured pending steps; subsequent ui.get reads the last payload sent to ui.set, not a merged or evaluated full sequence snapshot. Measurement Result starts as null; Measurement Table and Event Log start as empty arrays. ui.get(eventLogId) returns the last ui.set payload, not the complete accumulated log or any rows cleared from the GUI. For complete examples, open Examples > Getting Started > Automated Test Station."},
    {"ui.on", "on(widgetName, eventName, callback)", "Panel widget, event, callback", "", "ui.on(\"start\", \"click\", function() { print(\"Start\"); });", "Panel scripting API; only available with panel scripts."},
    {"plots.setSeries", "setSeries(id, name, xArray, yArray [, options]) : boolean", "Plot ID, name, matching numeric X/Y arrays, optional {color:'#RRGGBB'}", "User coordinates", "plots.setSeries(\"plot1\", \"Speed\", [0,1,2], [120,150,180]);", "Replaces only the named series on this plot. Other plot instances are not modified."},
    {"plots.setNativeSamples", "setNativeSamples(id, name, binarySamples, xStep [, xStart, options]) : boolean", "Uniformly sampled BinarySamples handle, positive finite X increment (in plot units), start X, optional series color/style", "User-defined X units", "const decoded=raw.decodeSamples('u16le',0,1000000,1.0,0.0); plots.setNativeSamples('scopeOverlay','CH1',decoded,0.00002,-10,{color:'#ffdc35'}); decoded.release();", "Native Qt shared-storage transfer; no per-sample JavaScript arrays. Renderer indexes all samples, draws visible-range chronology-preserving extrema and zooms to originals. Up to 4 million samples per series, 12 series per plot. Never mutate source after submission; release() is safe. Uniform linear X only; for arbitrary X continue using plots.setSeries()."},
    {"plots.nativeSeriesInfo", "nativeSeriesInfo(id,name) : object", "Panel plot ID, native series name", "X units", "const meta=plots.nativeSeriesInfo('scopeOverlay','CH1');print(meta.count,meta.xStart,meta.xStep);", "Returns native series metadata {native,count,xStart,xStep,xEnd}. Convert chart visible X bounds into sample indexes and call BinarySamples.rangeStats() for exact-range data. Empty object for non-native series."},
    {"plots.getView", "getView(id) : object", "Panel plot ID", "Chart units", "const view=plots.getView('scopeOverlay');", "Last asynchronous GUI view snapshot {xMin,xMax,yMin,yMax,isZoomed,seriesCount}. Initially empty until the user changes the view or plots.setView() is processed. Do not assume setView() is synchronous across threads."},
    {"plots.setView", "setView(id,xMin,xMax,yMin,yMax) : boolean", "Plot ID and four increasing finite axis limits", "Chart units", "plots.setView('scopeOverlay',-1,1,-2,2);", "Queues a native view change; triggers viewChanged after GUI applies it. For an automatic reset, double-click the chart."},
    {"plots.onViewChanged", "onViewChanged(id,callback) : void", "Plot ID, function(view)", "Chart units", "plots.onViewChanged('scopeOverlay', view=>print(view.xMin,view.xMax));", "Same event as ui.on(id,'viewChanged',callback); emitted for GUI wheel zoom, pan, double-click fit and scripted setView. Delivered asynchronously to the script worker; avoid expensive computations on every drag event."},
    {"BinarySamples.rangeStats", "rangeStats(offset,count) : object", "Zero-based start sample and sample count", "decoded sample units", "const r=samples.rangeStats(100,1000);print(r.min,r.max,r.rms);", "Native min/max/mean/rms on an exact requested range; zero JavaScript sample-array allocation. Compute on demand, not inside every zoom event for million-sample ranges."},
    {"plots.setSamples", "setSamples(id, name, samples, rateHz [, startSeconds, options]) : boolean", "Plot ID, name, Y samples, sample rate (Hz), start time", "s", "plots.setSamples(\"plot1\", \"CH1\", [0,.5,0], 1000, 0);", "Creates uniform time-axis X data without manually constructing an X array."},
    {"plots.fromScope", "fromScope(id [, channel=1, name='CH1']) : boolean", "Destination plot ID, Scope1 channel number 1/2, optional series name", "s, V", "if (!Scope1.wait(3)) throw Error(Scope1.lastError);\nplots.fromScope(\"plot1\",1,\"CH1\");", "No implicit auto-delivery to Panel plots; call this after Scope1.wait(). Scope workspace continues to update automatically."},
    {"plots.append", "append(id, name, xArray, yArray [, options]) : boolean", "Plot ID, series, X/Y batches", "User coordinates", "plots.append(\"trend\",\"RMS\",[1,2],[0.7,0.8]);", "Append bounded history, retaining newest 250000 points per series."},
    {"plots.appendPoint", "appendPoint(id, name, x, y) : boolean", "Plot ID, series, finite numeric X and Y", "User coordinates", "plots.appendPoint(\"trend\",\"RMS\",100,0.707);", "For occasional measurements; batch high-rate data with plots.append()."},
    {"plots.configure", "configure(id, options) : boolean", "Plot ID, chartType, labels, xAuto/yAuto, xMin/xMax, yMin/yMax, grid, legend, lineWidth, xLog/yLog, showCursors, cursorA/cursorB", "User coordinates", "plots.configure(\"plot1\",{chartType:\"XY\",xLabel:\"Angle\",yLabel:\"Torque\",yAuto:false,yMin:-10,yMax:10});", "Runtime settings are not saved to .wfpanel; edit the Properties inspector for saved defaults. RC1.5.2: legacyEnvelopeOpacity (0..1) controls the dim extrema envelope on dense ordinary X/Y Line plots; nativeEnvelopeOpacity controls native datasets. RC1.5.4: digitalPerfOverlay:true displays an opt-in Digital Timing frame-time HUD (frame, geometry, query, image blit)."},
    {"plots.styleSeries", "styleSeries(plotId, name, options) : boolean", "Options {color, width, markerSize, visible, axis: left or right}", "", "plots.styleSeries(\"plot1\",\"CH1\",{color:\"#49d2a1\",width:2.5});", "Changes an existing series without replacing its data."},
    {"plots.removeSeries", "removeSeries(id, name) : boolean", "Plot ID, existing series name", "", "plots.removeSeries(\"plot1\",\"CH1\");", "Removes only this plot's named series; the plot QWidget and other series survive."},
    {"plots.clear", "clear(id) : boolean", "Destination plot ID", "", "plots.clear(\"plot1\");", "Clears runtime chart data, not designer properties, the Page, or other plots."},
};
const Detail* detailFor(const QString& path) {
    for(const auto& d : kDetails)
        if(path==QLatin1String(d.path)) return &d;
    return nullptr;
}
QString esc(const QString& s) { return s.toHtmlEscaped(); }
}
QString canonical(QString p) {
    if(p==QStringLiteral("Supplies1") || p.startsWith(QStringLiteral("Supplies1.")))
        p.replace(0,9,QStringLiteral("Supplies"));
    for (const auto& base : {QStringLiteral("Wavegen"),QStringLiteral("Scope"),QStringLiteral("Logic"),QStringLiteral("Patterns"),QStringLiteral("Protocol")}) {
        if(p==base || p.startsWith(base+QLatin1Char('.'))) {
            p.replace(0,base.size(),base+QStringLiteral("1"));break;
        }
    }
    p.replace(QStringLiteral(".Channel2"),QStringLiteral(".Channel1"));
    return p;
}
bool isDocumentedSymbol(const QString& fullPath) {
    const QString p=canonical(fullPath);
    if(p.isEmpty()) return false;
    const int i=p.lastIndexOf(QLatin1Char('.'));
    const QString parent=i<0 ? QString() : p.left(i);
    const QString leaf=i<0 ? p : p.mid(i+1);
    for(const auto& entry:CompletionCatalog::entries(parent))
        if(entry.name==leaf) return true;
    return false;
}
Documentation lookup(const QString& fullPath) {
    const QString p=canonical(fullPath);
    Documentation out;out.path=p;
    if(!isDocumentedSymbol(p)) return out;
    const int i=p.lastIndexOf(QLatin1Char('.'));
    const QString parent=i<0 ? QString() : p.left(i);
    out.symbol=i<0 ? p : p.mid(i+1);
    for (const auto& entry : CompletionCatalog::entries(parent)) {
        if(entry.name==out.symbol) { out.summary=entry.description;break; }
    }
    out.known=true;
    if(const auto* d=detailFor(p)) {
        out.signature=QString::fromUtf8(d->signature);
        out.parameters=QString::fromUtf8(d->parameters);
        out.unit=QString::fromUtf8(d->unit);
        out.example=QString::fromUtf8(d->example);
        out.notes=QString::fromUtf8(d->notes);
    }
    return out;
}
QString html(const Documentation& d,bool dark) {
    const QString ink=dark?QStringLiteral("#edf4fc"):QStringLiteral("#17222f");
    const QString sub=dark?QStringLiteral("#bbcde0"):QStringLiteral("#40546a");
    const QString code=dark?QStringLiteral("#122132"):QStringLiteral("#edf3f9");
    QString s=QStringLiteral("<html><body style='color:%1;font-size:10pt'>").arg(ink);
    if(!d.known) return s+QStringLiteral("<b>No documented WFScript API symbol at this location.</b></body></html>");
    s+=QStringLiteral("<h3 style='margin:0 0 5px 0'>%1</h3>").arg(esc(d.path));
    if(!d.signature.isEmpty()) s+=QStringLiteral("<p><b>%1</b></p>").arg(esc(d.signature));
    s+=QStringLiteral("<p>%1</p>").arg(esc(d.summary));
    if(!d.parameters.isEmpty()) s+=QStringLiteral("<p><b>Arguments:</b> %1</p>").arg(esc(d.parameters));
    if(!d.unit.isEmpty()) s+=QStringLiteral("<p><b>Units:</b> %1</p>").arg(esc(d.unit));
    if(!d.example.isEmpty()) s+=QStringLiteral("<p style='color:%1'><b>Example</b></p><pre style='background:%2;padding:7px'>%3</pre>").arg(sub,code,esc(d.example));
    if(!d.notes.isEmpty()) s+=QStringLiteral("<p style='color:%1'>%2</p>").arg(sub,esc(d.notes));
    s+=QStringLiteral("<p style='color:%1;font-size:9pt'>WFScript supported API catalog; validate device capabilities at runtime.</p>").arg(sub);
    return s+QStringLiteral("</body></html>");
}
QString guideHtml(const QString& topic) {
    const QString intro=QStringLiteral("<html><body style='font-size:10pt;color:#203247'>"
        "<h3 style='margin-top:0'>WFScript Test Executive — %1</h3>%2"
        "<p style='color:#536578'><i>Reference: v5.6.63 project-owned C++ TestRunner. "
        "These pages explain concepts; use the TestRunner method nodes for exact signatures.</i></p></body></html>");
    QString content;
    if(topic==QStringLiteral("overview")) {
        content=QStringLiteral(
            "<p><b>There is one authoritative test configuration.</b> The Visual Test Sequence Editor "
            "stores its data in the .wfpanel project under testDefinition. The C++ runtime loads the "
            "definition and binds functions from the selected .mjs library BEFORE the operator script starts.</p>"
            "<p><b>To create a test:</b> Panel Designer &gt; Test Sequence... &gt; edit Setup, Main, Cleanup, "
            "totalTimeoutMs, timing, numeric limits and handler names. Save the .wfpanel. "
            "In the selected embedded JavaScript library, implement the named exported functions. "
            "For a numeric test return a number; C++ evaluates the limits.</p>"
            "<p><b>Operator code is just commands:</b></p>"
            "<pre>ui.on('runButton','clicked',function(){ TestRunner.run(); });\n"
            "ui.on('stopButton','clicked',function(){ TestRunner.terminate(); });\n"
            "TestRunner.onFinished(function(){ print(TestRunner.report().state); });</pre>"
            "<p>Do NOT call configure(), add(), or loadDefinition() from the operator script. "
            "They are disabled for project-managed sequences. The C++ executive owns the saved settings.</p>");
    } else if(topic==QStringLiteral("definitions")) {
        content=QStringLiteral(
            "<p>The .wfpanel project owns testDefinition (schemaVersion 1, id, title, config and steps) "
            "and testLibrary (name of one embedded .mjs library). The Visual Editor edits the project data "
            "directly, not JavaScript source. Save, reopen and run: the same document is used.</p>"
            "<p>A step has a group (setup/main/cleanup), execution mode (once/poll/delay), type "
            "(action/test/numeric), timeout and a named exported function. A numeric step additionally "
            "has limits:{low,high}, unit and optional resultWidget.</p>"
            "<pre>// In TestSteps.mjs:\nexport function readSupplyVoltage() { return 3.301; }\n"
            "// In the Visual Editor: Numeric step, handler=readSupplyVoltage,\n"
            "// low=3.25, high=3.35, unit=V. C++ determines PASS/FAIL.</pre>"
            "<p>The executive automatically validates names and connects the handlers when Panel mode starts. "
            "Missing exports are startup errors. An external .wftest.json export is useful for source control, "
            "but is not a second runtime configuration. definition() reads the saved test.</p>"
            "<p>Legacy manual add()/loadDefinition() APIs remain available only when there is no project-owned "
            "test definition. Do not use them for new Test Executive Panels.</p>");
    } else if(topic==QStringLiteral("groups")) {
        content=QStringLiteral(
            "<p><b>Setup:</b> initialize instruments, fixtures and the unit under test. "
            "<b>Main:</b> acquire/evaluate measurements. <b>Cleanup:</b> restore outputs, release resources.</p>"
            "<p>The executive orders groups Setup → Main → Cleanup regardless of registration order. "
            "When stopOnFailure is true, remaining Main tests become SKIPPED, then Cleanup executes. "
            "Runtime ERROR and total/step timeouts also lead to Cleanup. Cleanup is cooperative, not "
            "a guarantee when an SDK call blocks or the process is terminated.</p>"
            "<p><b>Step type:</b> action completes as DONE (or ERROR); test completes as PASS/FAIL. "
            "For numeric steps the function returns a number; C++ applies limits and records the reading. A failed DUT limit is FAIL; an exception or timed-out test procedure is ERROR.</p>");
    } else if(topic==QStringLiteral("modes")) {
        content=QStringLiteral(
            "<p><b>once:</b> call one handler synchronously and evaluate its boolean result. "
            "<b>delay:</b> nonblocking Qt-timed wait using durationMs; no JS handler is required. "
            "<b>poll:</b> optional start(), repeated poll(stepElapsedMs,totalElapsedMs), optional "
            "finish(status,elapsedMs). Return undefined/null to continue, true to PASS/DONE, false to FAIL.</p>"
            "<pre>TestRunner.add({id:'settle',name:'Settle',group:'setup',type:'action',\n"
            "    mode:'delay',durationMs:150,timeoutMs:600},null);\n"
            "TestRunner.add({id:'clock',name:'Clock',mode:'poll',\n"
            "    intervalMs:10,timeoutMs:800},\n"
            "    {start:steps.beginClock,poll:steps.pollClock,finish:steps.endClock});</pre>"
            "<p>Polling intervals of 10 ms are best effort, not real time; the timer cannot preempt "
            "a synchronous JavaScript/SDK call. Step timeoutMs and totalTimeoutMs are deadlines checked "
            "when the worker returns control to Qt.</p>");
    } else if(topic==QStringLiteral("lifecycle")) {
        content=QStringLiteral(
            "<p><b>run():</b> start or resume execution; for timed steps a true return means "
            "accepted, not final PASS. <b>next():</b> execute one step; a timed step may remain RUNNING. "
            "Use onFinished(callback) or state()/summary() for terminal outcomes.</p>"
            "<p><b>terminate():</b> graceful operator stop, remaining Main steps skipped, Cleanup runs, "
            "overall ABORTED. <b>abort():</b> emergency stop, sequence-level Cleanup skipped. "
            "<b>reset():</b> reset step/results; <b>clear():</b> discard definitions.</p>"
            "<p>The status vocabulary is idle, running, passed, failed, error, aborted for the overall "
            "execution; individual steps use pending, running, pass, done, fail, error, skipped, aborted.</p>"
            "<p><b>Safety:</b> forced process stop, interrupted QJSEngine or blocked hardware driver can "
            "prevent script Cleanup. Design electrical interlocks and safe hardware defaults separately.</p>");
    } else if(topic==QStringLiteral("context")) {
        content=QStringLiteral(
            "<p>setRunInfo({uutId,stationId,operatorId,...}) captures primitive metadata before each "
            "execution. setVariable(name,value) and variable(name) exchange bounded primitive values "
            "between library steps while executing. report() includes runInfo and variables.</p>"
            "<pre>TestRunner.setRunInfo({uutId:'UUT-001',operatorId:'operator'});\n"
            "// Inside a running step handler:\n"
            "TestRunner.setVariable('voltage',3.301);\n"
            "// Inside another step handler:\n"
            "const voltage=TestRunner.variable('voltage');</pre>"
            "<p>Metadata is not saved in a .wfpanel and run variables are cleared when starting "
            "another execution. Keep measurements attached to their originating step with measure().</p>");
    } else if(topic==QStringLiteral("reports")) {
        content=QStringLiteral(
            "<p>TestRunner.report() provides a C++ snapshot: executionId, state, UTC timestamps, "
            "elapsedMs, runInfo, variables, step records and measurement rows. An out-of-limits "
            "measurement is stored with FAIL and the step ID. The native report remains "
            "in memory; when automatic archiving is enabled in Test Sequence > Report Storage, "
            "it is saved as JSON after Cleanup at terminal state.</p>"
            "<pre>TestRunner.onFinished(function(info){\n"
            "    const report=TestRunner.report();\n"
            "    print(report.executionId,info.state);\n"
            "    const archive=TestRunner.archiveInfo();\n"
            "    print(archive.status,archive.path,archive.error);\n"
            "});\n"
            "// After the run has finished:\n"
            "const path=TestRunner.exportReport('json');\n"
            "// Or choose an existing folder yourself:\n"
            "TestRunner.saveReport('C:/TestReports/UUT-001.csv','csv');</pre>"
            "<p>exportReport() writes to Documents/WFScript Reports with a UUT/UUID filename. "
            "JSON contains the complete native record. CSV contains STEP and MEASUREMENT rows, "
            "with stepId linking each measurement to its step. Files are written atomically. "
            "Export rejects idle/running tests.</p>");
    } else if(topic==QStringLiteral("libraries")) {
        content=QStringLiteral(
            "<p>Use the existing Script Libraries manager to store reusable .mjs files, or embed "
            "a module inside a .wfpanel. modules.load('TestStationSteps.mjs') returns module "
            "exports from the same QJSEngine that owns TestRunner; imported callbacks may call "
            "TestRunner.measure(), setVariable() and other normal script APIs.</p>"
            "<p>Keep test functions short and nonblocking. The executive schedules the callbacks; "
            "your library implements the hardware measurement and returns status. "
            "See Examples → Getting Started → Automated Test Station.</p>");
    }
    if(content.isEmpty())return {};
    const QHash<QString,QString> titles={
        {QStringLiteral("overview"),QStringLiteral("Getting Started")},
        {QStringLiteral("definitions"),QStringLiteral("Versioned Sequence Definitions")},
        {QStringLiteral("groups"),QStringLiteral("Setup, Main, Cleanup & Step Types")},
        {QStringLiteral("modes"),QStringLiteral("Execution Modes & Timers")},
        {QStringLiteral("lifecycle"),QStringLiteral("Execution Lifecycle & Safety")},
        {QStringLiteral("context"),QStringLiteral("Run Context & UUT Metadata")},
        {QStringLiteral("reports"),QStringLiteral("Results & Report Export")},
        {QStringLiteral("libraries"),QStringLiteral("Reusable Script Libraries")}};
    return intro.arg(titles.value(topic,topic),content);
}
QStringList curatedPaths() {
    QStringList paths;
    for(const auto& d:kDetails) paths.append(QString::fromLatin1(d.path));
    return paths;
}
QVector<Snippet> snippets() {
    return {
        {QStringLiteral("binarypacket"),QStringLiteral("Native binary packet read/write round-trip"),QStringLiteral(
            "const pkt=Binary.create(16);\n"
            "pkt.writeU16BE(0,${1:0x1234});\n"
            "pkt.writeF32LE(2,${2:3.25});\n"
            "pkt.writeU16LE(6,pkt.crc16Range(0,6));\n"
            "print(pkt.toHex(0,8),pkt.readF32LE(2));\n")},
        {QStringLiteral("binarydbc"),QStringLiteral("CAN DBC Intel and Motorola bit packing"),QStringLiteral(
            "const can=Binary.create(8);\n"
            "can.writeSignal(${1:0},${2:16},'intel',false,0.1,-40,${3:23.5});\n"
            "print(can.readSignal(0,16,'intel',false,0.1,-40));\n"
            "// DBC database files, multiplexing and CAN FD hardware are separate.\n")},
        {QStringLiteral("binaryscpi"),QStringLiteral("Native framed IEEE binary blocks"),QStringLiteral(
            "const rx=Binary.stream(${1:4000000});\n"
            "// In an existing TCP.on('binary', function(chunk) {...}) callback:\n"
            "if(!rx.append(${2:chunk}))throw Error(rx.lastError);\n"
            "const payload=rx.readIEEE();\n"
            "if(payload){ const stats=payload.statsU16LE(0,payload.length/2); print(stats.rms); payload.release(); }\n")},
        {QStringLiteral("datawatch"),QStringLiteral("Subscribe to a Panel Data value"),QStringLiteral(
            "const changeId = Data.onChange('${1:Motor.ActualRPM}', function(value, previousValue, name) {\n"
            "    print(name + ': ' + previousValue + ' -> ' + value);\n"
            "}, {immediate:true});\n"
            "// Later if needed: Data.offChange(changeId);\n")},
        {QStringLiteral("datacomputed"),QStringLiteral("Calculated, read-only Panel Data value"),QStringLiteral(
            "// Predeclare ${1:Motor.TargetRPS} as Number in Properties > Panel Data.\n"
            "Data.computed('${1:Motor.TargetRPS}', ['${2:Motor.TargetRPM}'], function(rpm) {\n"
            "    return Number((rpm / 60).toFixed(2));\n"
            "});\n"
            "// Bind a display to the calculated target; Data.set(target,...) is blocked.\n")},
        {QStringLiteral("paneltimer"),QStringLiteral("Panel timer: repeated task with cleanup"),QStringLiteral(
            "let heartbeat = setInterval(function() {\n"
            "    print('tick'); // keep this callback short\n"
            "}, ${1:1000});\n"
            "// Later: clearInterval(heartbeat);\n"
            "setTimeout(function(message){print(message);}, ${2:2000}, 'ready');\n")},
        {QStringLiteral("wgsine"),QStringLiteral("Configure and start a sine generator"),QStringLiteral(
            "Wavegen.Channel1.enable = true;\n"
            "Wavegen.Channel1.Mode.text = \"Simple\";\n"
            "Wavegen.Channel1.Simple.Type.text = \"Sine\";\n"
            "Wavegen.Channel1.Simple.Frequency.value = ${1:1000};\n"
            "Wavegen.Channel1.Simple.Amplitude.value = ${2:1.0};\n"
            "Wavegen.Channel1.Simple.Offset.value = 0;\n"
            "if (!Wavegen.run()) throw new Error(Wavegen.lastError);\n")},
        {QStringLiteral("scopecapture"),QStringLiteral("Single triggered acquisition and RMS"),QStringLiteral(
            "Scope.Time.Rate.value = ${1:1000000};\n"
            "Scope.Time.Samples.value = ${2:16384};\n"
            "if (!Scope.single()) throw new Error(Scope.lastError);\n"
            "if (!Scope.wait(5)) throw new Error(Scope.lastError);\n"
            "var rms = Scope.Channel1.measure(\"RMS\");\n"
            "print(\"CH1 RMS:\", rms);\n")},
        {QStringLiteral("scopefft"),QStringLiteral("Run a native FFT on acquired Scope data"),QStringLiteral(
            "var fft = Scope.Channel1.spectrum(${1:16384}, \"${2:Hann}\");\n"
            "print(\"FFT peak frequency (Hz):\", fft.peakFrequencyHz);\n")},
        {QStringLiteral("scoperef"),QStringLiteral("Save a baseline and subtract a reference"),QStringLiteral(
            "Scope.saveReference(1);\n"
            "// After acquiring a second frame:\n"
            "var difference = Scope.mathData(\"CH1-REF1\");\n"
            "print(\"Reference difference samples:\", difference.length);\n")},
        {QStringLiteral("scoperecord"),QStringLiteral("Record-mode scope acquisition (check capabilities)"),QStringLiteral(
            "Scope.Time.Mode.text = \"Record\";\n"
            "Scope.Time.Rate.value = ${1:200000};\n"
            "Scope.Time.RecordLength.value = ${2:0.5};\n"
            "if (!Scope.single()) throw new Error(Scope.lastError);\n"
            "if (!Scope.wait(10)) throw new Error(Scope.lastError);\n"
            "print(\"Samples:\", Scope.captureInfo().sampleCount);\n")},
        {QStringLiteral("scopewindow"),QStringLiteral("Measure RMS inside a selected time window"),QStringLiteral(
            "var rms = Scope.Channel1.measureWindow(\"RMS\", ${1:0}, ${2:0.05});\n"
            "print(\"Window RMS:\", rms);\n")},
        {QStringLiteral("eventlog"),QStringLiteral("Append timestamped messages to an Event Log widget"),QStringLiteral(
            "// Add an Event Log widget named stationLog; displays and filters entries.\n"
            "ui.set('stationLog',{append:{level:'INFO',source:'Test',message:'Starting check'}});\n"
            "ui.set('stationLog',{append:{level:'PASS',source:'Test',message:'Voltage within tolerance'}});\n"
            "// ui.set('stationLog',{clear:true}); // clear event history\n")},
                {QStringLiteral("testpoll"),QStringLiteral("Native 10ms polling step with imported library"),QStringLiteral(
            "const checks=modules.load('TestStationSteps.mjs');\n"
            "TestRunner.onFinished(function(info){ print(info.state, info.elapsedMs); });\n"
            "TestRunner.add({id:'clock',name:'Clock check',mode:'poll',intervalMs:10,timeoutMs:800},\n"
            "    {start:checks.beginClock,poll:checks.pollClock,finish:checks.endClock});\n"
            "// Start from a button event: TestRunner.run();\n")},
{QStringLiteral("testrunner"),QStringLiteral("Define and execute steps with native Qt/C++ TestRunner"),QStringLiteral(
            "// C++ owns the execution state and updates test widgets.\n"
            "if(!TestRunner.configure({sequenceId:'stationSequence',tableId:'stationTable',logId:'stationLog',stopOnFailure:true}))\n"
            "    throw Error(TestRunner.lastError);\n"
            "if(!TestRunner.add({id:'supply',name:'Verify supply',group:'main',type:'test'},function(){\n"
            "    var measured=${1:3.301}; // replace with actual hardware measurement\n"
            "    return TestRunner.measure('Supply',measured,3.25,3.35,'V','resultPositive');\n"
            "}))throw Error(TestRunner.lastError);\n"
            "// Start from an event: TestRunner.run();\n")},
        {QStringLiteral("teststeps"),QStringLiteral("Drive the Test Sequence visual widget from JavaScript"),QStringLiteral(
            "// Visual state ONLY. Define a Test Sequence widget named stationSequence.\n"
            "ui.set('stationSequence',[{id:'power',name:'Check supply',status:'pending'}]);\n"
            "ui.set('stationSequence',{update:{id:'power',status:'running',message:'Measuring...'}});\n"
            "// Run your own SDK acquisition and evaluate limits here.\n"
            "ui.set('stationSequence',{update:{id:'power',status:'pass',elapsedMs:125,message:'3.301 V'}});\n"
            "// ui.set('stationSequence',{reset:true}); // keep steps, clear result states\n")},
        {QStringLiteral("patternsequence"),QStringLiteral("Generate synchronized repeating 3-bit digital patterns"),QStringLiteral(
            "// AD3: 3.3V outputs, connect DIO0->DIO3, DIO1->DIO4, DIO2->DIO5 via 1k.\n"
            "if(!StaticIO.stop()) throw Error(StaticIO.lastError);\n"
            "if(!Patterns1.runSequence([0,1,2],[\"01010101\",\"00110011\",\"00001111\"],${1:1000})) throw Error(Patterns1.lastError);\n"
            "try {\n"
            "  print(\"Actual bit rate:\",Patterns1.status().sequenceActualBitRateHz);\n"
            "  Logic1.Time.Rate.value=32000; Logic1.Time.Samples.value=4096;\n"
            "  if(!Logic1.single() || !Logic1.wait(5)) throw Error(Logic1.lastError);\n"
            "  print(\"Digital samples:\",Logic1.samples().length);\n"
            "} finally { if(!Patterns1.stop()) print(\"STOP FAILED:\",Patterns1.lastError); }\n")},
        {QStringLiteral("encoderindex"),QStringLiteral("Generate 3-channel hardware quadrature encoder with index"),QStringLiteral(
            "// AD3: 3.3V outputs. Connect DIO0->DIO3, DIO1->DIO4, DIO2->DIO5 through 1k resistors.\n"
            "if(!StaticIO.stop()) throw Error(StaticIO.lastError);\n"
            "if(!Patterns1.runEncoder(0,1,2,${1:16},${2:600},${3:false})) throw Error(Patterns1.lastError);\n"
            "try {\n"
            "  print('Actual RPM:',Patterns1.status().encoderActualRpm);\n"
            "  // Capture DIO3 (A), DIO4 (B), DIO5 (index) with Logic1.\n"
            "  Logic1.Time.Rate.value=17000; Logic1.Time.Samples.value=4096;\n"
            "  if(!Logic1.single() || !Logic1.wait(5)) throw Error(Logic1.lastError);\n"
            "  print('Digital input samples:',Logic1.samples().length);\n"
            "} finally { if(!Patterns1.stop()) print('STOP FAILED:',Patterns1.lastError); }\n")},
        {QStringLiteral("patternclock"),QStringLiteral("Start DIO0 hardware clock/PWM and capture with Logic1"),QStringLiteral(
            "// Connect AD3 DIO0 to DIO1 through 1 kOhm resistor first.\n"
            "StaticIO.stop(); // release StaticIO drivers\n"
            "Patterns1.Channels.DIO0.Pin.value=0;\n"
            "Patterns1.Channels.DIO0.Type.text='Clock';\n"
            "Patterns1.Channels.DIO0.Output.text='PP';\n"
            "Patterns1.Channels.DIO0.Idle.text='Z';\n"
            "Patterns1.Channels.DIO0.Frequency.value=${1:1000};\n"
            "Patterns1.Channels.DIO0.Duty.value=${2:50};\n"
            "if(!Patterns1.run()) throw Error(Patterns1.lastError);\n"
            "try {\n"
            "  Logic1.Time.Rate.value=100000; Logic1.Time.Samples.value=4096;\n"
            "  if(!Logic1.single() || !Logic1.wait(3)) throw Error(Logic1.lastError);\n"
            "  print('Samples:',Logic1.samples().length);\n"
            "} finally { if(!Patterns1.stop()) print('STOP FAILED:',Patterns1.lastError); }\n")},
        {QStringLiteral("logiccapture"),QStringLiteral("Capture digital input DIO1 and report transitions"),QStringLiteral(
            "Logic1.Time.Rate.value = ${1:20000};\n"
            "Logic1.Time.Samples.value = ${2:4096};\n"
            "if (!Logic1.single()) throw Error(Logic1.lastError);\n"
            "if (!Logic1.wait(3)) throw Error(Logic1.lastError);\n"
            "var words=Logic1.samples();\n"
            "var levels=words.map(function(w){return (w >> 1) & 1;});\n"
            "print('DIO1 samples:',levels.length,'rate:',Logic1.sampleRateHz);\n")},
        {QStringLiteral("digitalio"),QStringLiteral("AD3 DIO0 high / DIO1 input / safe cleanup"),QStringLiteral(
            "// Connect DIO0 to DIO1 through 1 kOhm resistor first.\n"
            "StaticIO.Channel0.DIO0.value = 1;\n"
            "StaticIO.Channel0.DIO1.value = 2; // Z, input\n"
            "if(!StaticIO.config()) throw Error(StaticIO.lastError);\n"
            "var s=StaticIO.status();\n"
            "if(!s.valid) throw Error(s.lastError);\n"
            "print('DIO1:', (s.inputMask >> 1) & 1);\n"
            "if(!StaticIO.stop()) print('HIGH-Z NOT CONFIRMED:', StaticIO.lastError);\n")},
        {QStringLiteral("ad3supplies"),QStringLiteral("Start AD3 positive and negative DC supplies"),QStringLiteral(
            "Supplies.Output.PositiveSupply.Voltage.value = ${1:3.3};\n"
            "Supplies.Output.NegativeSupply.Voltage.value = ${2:-3.3};\n"
            "Supplies.Output.PositiveSupply.Enable.value = 1;\n"
            "Supplies.Output.NegativeSupply.Enable.value = 1;\n"
            "if (!Supplies.run()) throw Error(Supplies.lastError);\n"
            "print(Supplies.status().measuredAvailable);\n"
            "// Use Supplies.stop() when done.\n")},
        {QStringLiteral("checkdevice"),QStringLiteral("Verify a device is available"),QStringLiteral(
            "if (!Device.isConnected()) {\n"
            "    throw new Error(\"No device connected\");\n"
            "}\n")},
        {QStringLiteral("trycatch"),QStringLiteral("JavaScript error handling"),QStringLiteral(
            "try {\n"
            "    ${1:// Insert code here}\n"
            "} catch (error) {\n"
            "    print(\"Script error:\", error);\n"
            "}\n")}
    };
}
Snippet snippet(const QString& trigger) {
    for(const auto& s:snippets()) if(s.trigger==trigger) return s;
    return {};
}
QStringList referencedApiPaths(const QString& snippetBody) {
    QStringList found;
    static const QRegularExpression rx(QStringLiteral(R"(\b(?:Scope|Wavegen|Logic1|Logic|Supplies|StaticIO|Patterns1|Patterns|Protocol1|Protocol|Device|TestRunner)(?:\.[A-Za-z_]\w*)+)"));
    auto match=rx.globalMatch(snippetBody);
    while(match.hasNext()) {
        const QString p=match.next().captured();
        if(!found.contains(p)) found.append(p);
    }
    return found;
}
}
