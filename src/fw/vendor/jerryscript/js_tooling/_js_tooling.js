// This file will be concatenated to the end of the cross-compiled compiler
// it's been written in a way such that
// 1. (development) it can be used to develop the functions and call require the cross-compiled version
// 2. concatenated into the actual compiler as either
// 2a.) (CLI) ...an npm module so that the functionality is exposed as functions via CommonJS
// 2b.) (plain JS) ...as a standalone JS file to be used in a browser or on an empty JS context

(function(global, jerry){

if (typeof jerry === 'undefined') {
    // in development environment (if uses include _js_tooling.js instead of js_tooling.js) we use this indirection
    // to write wrapper code without the need to re-run Emscripten
    jerry = require('../../../../../build/src/fw/vendor/jerryscript/js_tooling/js_tooling.js');
}

// size_t
// jerry_parse_and_save_snapshot_from_zt_utf8_string (
//    const jerry_char_t *zt_utf8_source_p, /**< zero-terminated UTF-8 script source */
//    bool is_for_global, /**< snapshot would be executed as global (true)
//    bool is_strict, /**< strict mode */
//    uint8_t *buffer_p, /**< buffer to save snapshot to */
//    size_t buffer_size) /**< the buffer's size */

var jerry_parse_and_save_snapshot_from_zt_utf8_string = function(zt_utf8_source_p, is_for_global, is_strict, buffer_p, buffer_size) {
    return jerry['ccall'](
        'jerry_parse_and_save_snapshot_from_zt_utf8_string',
        'number',
        ['string', 'number', 'number', 'number', 'number'],
        [zt_utf8_source_p, is_for_global, is_strict, buffer_p, buffer_size]);
};

// uint32_t legacy_defective_checksum_memory(const void * restrict data, size_t length);
var legacy_defective_checksum_memory = function(data, length) {
    return jerry['ccall'](
        'legacy_defective_checksum_memory',
        'number',
        ['number', 'number'],
        [data, length]
    );
};

// size_t size_t rocky_fill_header(uint8_t *buffer, size_t buffer_size);
var rocky_fill_header = function(buffer, buffer_size) {
        return jerry['ccall'](
            'rocky_fill_header',
            'number',
            ['number', 'number'],
            [buffer, buffer_size]
        );
};

// void jerry_port_set_errormsg_handler(JerryPortErrorMsgHandler handler)
var jerry_port_set_errormsg_handler = function(funcPtr) {
    return jerry['ccall'](
        'jerry_port_set_errormsg_handler',
        'void',
        ['number'],
        [funcPtr]
    );
};

var malloc = jerry['_malloc'];
var memset = jerry['_memset'];
var getValue = jerry['getValue'];
var setValue = jerry['setValue'];
var free = jerry['_free'];

function error(reason) {
    return {
        'result': 'error',
        'reason': reason
    };
}

// helper functions for logging timings
var captureLevel = 0;
function captureDuration(msg) {
    var d = new Date();
    d.msg = msg;
    d.level = captureLevel++;
    return d;
}

var JS_TOOLING_LOGGING;
function logDuration(d, msg) {
    if (typeof(JS_TOOLING_LOGGING) === 'undefined' || !JS_TOOLING_LOGGING) {
        return;
    }

    var indentation = '';
    for (var i = 0; i < d.level; i++) {
        indentation = '    ' + indentation;
    }

    var duration = Math.floor((new Date().getTime()-d.getTime())) + 'ms';
    while (duration.length < 7) {
        duration = ' ' + duration;
    }
    console.log(indentation + duration + ' - '+ d.msg);
    captureLevel--;
}

var defaultSnapshotMaxSize = 24 * 1024;

function createSnapshot(js, options) {
    var timeCreateSnapshot = captureDuration('createSnapshot');
    options = options || {};
    options.maxsize = Math.max(0, options.maxsize || defaultSnapshotMaxSize);
    options.padding = Math.max(0, options.padding || 0);

    js += '\n'; // work around: JerryScript sometimes cannot handle missing \n at EOF

    var bufferAlignment = 8;
    var bufferAlignmentMinus = bufferAlignment - 1;
    var bufferSize = 256 * 1024;
    if (options.maxsize > bufferSize) {
        return error('maxsize (' + options.maxsize + ') cannot exceed ' + bufferSize);
    }

    var buffer = malloc(bufferSize + bufferAlignmentMinus);

    memset(buffer, 0, bufferSize + bufferAlignmentMinus);
    var alignedBuffer = Math.floor((buffer + bufferAlignmentMinus) / bufferAlignment) * bufferAlignment;

    // add (default) prefix to the buffer
    var prefixLen;
    if (typeof options.prefix !== 'string') {
        prefixLen = rocky_fill_header(alignedBuffer, bufferSize);
    } else {
        prefixLen = options.prefix.length;
        for (var i = 0; i < prefixLen; i++) {
            setValue(alignedBuffer + i, options.prefix.charCodeAt(i), 'i8');
        }
    }
    if (prefixLen % 8 != 0) {
        return error('length of prefix must be divisible by 8')
    }

    var jerryBufferStart = alignedBuffer + prefixLen;
    var jerryMaxBufferSize = bufferSize - prefixLen;

    var timeJerryInit = captureDuration('jerry_init');
    jerry['_jerry_init'](0);
    logDuration(timeJerryInit);

    var collectedErrors = [];
    var errorHandlerPtr = jerry['Runtime'].addFunction(function(msgPtr) {
        var msg = jerry['Pointer_stringify'](msgPtr).trim();
        if (msg !== 'Error:') {
            collectedErrors.push(msg);
        }
        return true;
    });
    jerry_port_set_errormsg_handler(errorHandlerPtr);

    var timeJerry = captureDuration('jerry_parse_and_save_snapshot');
    try {
        var jerryUsedBuffer = jerry_parse_and_save_snapshot_from_zt_utf8_string(
                js, 1, 0, jerryBufferStart, jerryMaxBufferSize);
    } catch(e) {
        if (collectedErrors.length == 0) {
            // in case no other error was logged through JerryScript we will at least have the Exit code
            collectedErrors.push(e.message.trim());
        }
        return error(collectedErrors.join('. '));
    }
    logDuration(timeJerry);
    jerry['Runtime'].removeFunction(errorHandlerPtr);

    var timeJerryCleanup = captureDuration('jerry_cleanup');
    jerry['_jerry_cleanup']();
    logDuration(timeJerryCleanup);

    // TODO: free buffer once we know how to do that reliably
    if (jerryUsedBuffer == 0) {
        if (collectedErrors.length === 0) {
            return error('JS compilation error (no further details available)');
        } else {
            return error(
                'JS compilation error(s): ' + collectedErrors.join(', '));
        }
    }

    var timeArrayConversion = captureDuration('snapshot array conversion');

    var snapshotLen = jerryUsedBuffer + prefixLen + options.padding;
    if (snapshotLen > options.maxsize) {
        return error('snapshot size ' + snapshotLen + ' exceeds maximum size ' + options.maxsize);
    }

    var b = new Array(snapshotLen);
    // add actual snapshot data
    for (i = 0; i < snapshotLen; i++) {
        // TODO: should we shift this to 0..255 by adding 128
        b[i] = getValue(alignedBuffer + i, 'i8');
    }
    logDuration(timeArrayConversion);

    // TODO: Emscripten MEMORY LEAK - buffer must be freed here but calling
    // jerry._free(buffer);
    // here fails when calling the function the second time

    logDuration(timeCreateSnapshot);
    return {
        'result': 'success',
        'snapshot': b,
        'snapshotSize': snapshotLen,
        'snapshotMaxSize': options.maxsize
    };
}

function patchPBPack(js, pbpack, options) {
    var timepatchPBPack = captureDuration('patchPBPack');
    function readUInt32(offset) {
        var result = 0;
        for (var i = 0; i < 4; i++) {
            result |= pbpack[offset + i] << (i * 8);
        }
        return result;
    }

    function writeUInt32(offset, value) {
        for (var i = 0; i < 4; i++) {
            pbpack[offset + i] = (value >> (i * 8)) & 0xff;
        }
    }

    var ENTRIES_OFFSET = 0x0C;
    var CONTENT_OFFSET = 0x100C;

    function offsetsForEntry(entry) {
        var entryOffset = ENTRIES_OFFSET + entry * 16;

        return {
            'size': entryOffset + 8,
            'content': CONTENT_OFFSET + readUInt32(entryOffset + 4)
        };
    }


    function findPJSEntry(numEntries) {
        for (var entry = 0; entry < numEntries; entry++) {
            var offsets = offsetsForEntry(entry);
            if (readUInt32(offsets.size) >= 4) {
                var first4bytes = readUInt32(offsets.content);
                if (first4bytes == 0x00534a50) { // 'PJS\0'==0x50,0x4A,0x53,x00 in with correct endian
                    return offsets;
                }
            }
        }
        return undefined;
    }

    var numEntries = readUInt32(0x00);
    if (numEntries > 256) return error('pbpack contains more than 256 entries: ' + numEntries);

    var pjsEntryOffsets = findPJSEntry(numEntries);
    if (typeof pjsEntryOffsets === 'undefined') {
        return error('could not find resource to patch in ' + numEntries + ' entries');
    }

    // TODO: implement shortcut that skips the step if versions match
    var snapshot = createSnapshot(js, options);
    if (snapshot['result'] !== 'success') return error(snapshot['reason']);
    snapshot = snapshot['snapshot'];

    var requiredSpace = snapshot.length;
    var availableSpace = readUInt32(pjsEntryOffsets.size);
    if (availableSpace < requiredSpace) {
        return error('required byte size (' + requiredSpace + ') for resource exceeds maximum (' + availableSpace + ')');
    }

    var timeCopySnapshot = captureDuration('copy snapshot to pbpack');
    for(var i = 0; i < snapshot.length; i++) {
        pbpack[pjsEntryOffsets.content + i] = snapshot[i];
    }
    logDuration(timeCopySnapshot);

    // ---- calc CRC
    var timeCRC = captureDuration('calc CRC');
    var CRC_OFFSET = 0x4;
    var numCheckedBytes = pbpack.length - CONTENT_OFFSET;
    var buffer = malloc(numCheckedBytes);
    for(i = CONTENT_OFFSET; i < pbpack.length; i++) {
        var value = pbpack[i];
        setValue(buffer - CONTENT_OFFSET + i, value, 'i8');
    }
    var crcResult = legacy_defective_checksum_memory(buffer, numCheckedBytes);
    // console.log('CRC 0x' + crcResult.toString(16));
    free(buffer);
    writeUInt32(CRC_OFFSET, crcResult);
    logDuration(timeCRC);

    logDuration(timepatchPBPack, 'PBPack');
    return {
        'result': 'success',
        'pbpack': pbpack
    };
}

var exports = global; // default, in case we are not running inside a node instance

if (typeof module !== 'undefined' && module.exports) {
    exports = module.exports;
}

exports['createSnapshot'] = createSnapshot;
exports['patchPBPack'] = patchPBPack;
exports['defaultSnapshotMaxSize'] = defaultSnapshotMaxSize;

})(this, (typeof Module !== 'undefined') ? Module : undefined);
