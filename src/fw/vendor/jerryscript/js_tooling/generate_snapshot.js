#!/usr/bin/env node

// generate_snapshot.js is a barebones tool for creating snapshots from within an automated process,
// with the goal of eliminating build-time dependencies in exchange for a more narrow set of
// capabilities

var fs = require('fs');
var jsCompiler = require('./js_tooling');

function writeToFile(argv, dataToWrite) {
    var data;
    if (Array.isArray(dataToWrite)) {
        data = byteArrayToStr(dataToWrite);
    }
    else if (typeof dataToWrite == 'object') {
        data = JSON.stringify(dataToWrite);
    }
    else {
        data = dataToWrite;
    }
    fs.writeFileSync(argv.output, data, 'binary');
}

function byteArrayToStr(arr) {
    var result = '';
    for (i = 0; i < arr.length; i++) {
        result += String.fromCharCode(arr[i]);
    }
    return result;
}

if (require.main === module) {
    if (process.argv.length < 4) {
        console.log("Usage: " + __filename + " JS_FILE OUTPUT [MEMORY_REPORT]");
        process.exit(-1);
    }
    var js_file = process.argv[2];
    var output_file = process.argv[3];

    var snapshot_size_file = 'snapshot_size.json';
    if (process.argv.length == 5) {
        snapshot_size_file = process.argv[4];
    }

    var expectedArgs = {
        prefix: function(v) {return decodeURIComponent(v)},
        maxsize: function(v) {return parseInt(v)},
        padding: function(v) {return parseInt(v)}
    };

    var options = {};
    Object.keys(expectedArgs).forEach(function(k) {
        var idx = process.argv.indexOf('--' + k);
        if (idx >= 0) {
            options[k] = expectedArgs[k](process.argv[idx+1]);
        }
    });

    var js = fs.readFileSync(js_file, 'utf8');
    var result = jsCompiler.createSnapshot(js, options);

    if (result.result != 'success') {
        console.error(result['result'] + ':', result['reason']);
        process.exit(1);
    } else {
        writeToFile({'output': output_file}, result['snapshot']);

        // This call writes out the bytecode memory usage for
        // /tools/resources/resource_map/resource_generator.py to read. Please update that file
        // if you make changes to the output format/filename here
        var snapshot_size_data = {size: result['snapshotSize'], max: result['snapshotMaxSize']};
        writeToFile({'output': snapshot_size_file}, snapshot_size_data);
    }
}
