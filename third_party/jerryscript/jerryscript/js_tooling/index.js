#!/usr/bin/env node

// entry point for node-based CLI that produces snapshots (e.g. Pebble SDK)

var yargs = require('yargs');
var jsCompiler = require('./js_tooling');
var fs = require('fs');
var zip = require('node-zip');

yargs
    .command('compile', 'compiles JavaScript to byte code', function(yargs) {
        return defaultOptions(yargs);
    }, compile)
    .command('patch', 'patches an existing PBPack or PBW file', function(yargs) {
        return defaultOptions(yargs)
            .option('pbpack', {
                describe: 'PBPack file to patch',
                type: 'string',
            })
            .option('pbw', {
                describe: 'PBW file to patch',
                type: 'string',
            })
            .check(function(argv, arr) {
                if (typeof (argv.pbw) == typeof (argv.pbpack)) {
                    throw 'Need to specifiy either --pbw or --pbpack'
                }
                return true;
            });
    }, patch)
    .help()
    .detectLocale(false) // force english
    .check(function(argv, arr) {
        throw "No command provided."
    })
    .argv;

function defaultOptions(yargs) {
    return yargs
        .option('js', {
            describe: 'JavaScript input file (or - for stdin)',
            demand: true,
            type: 'string'
        })
        .option('prefix', {
            describe: 'Non-standard sequence of bytes to prepend snapshot with (passed through decodeURIComponent())',
            type: 'string'
        })
        .option('maxsize', {
            describe: 'Maximum number of bytes the resulting snapshot (including padding) can be',
            type: 'number',
            default: jsCompiler.defaultSnapshotMaxSize
        })
        .option('padding', {
            describe: 'Number of bytes to add to the snapshot for padding',
            type: 'number',
            default: 0
        })
        .option('output', {
            describe: 'output file (or - for stdout)',
            demand: true,
            type: 'string'
        })
        .strict()
        .check(function(argv, arr) {
            if (argv._.length != 1) {
                throw 'Ambiguous command provided: "' + argv._.join(' ') + '"'
            }
            return true;
        });
}

function snapshotOptions(argv) {
    return {
        prefix: argv.prefix ? decodeURIComponent(argv.prefix) : undefined,
        maxsize: argv.maxsize,
        padding: argv.padding
    };
}

function compile(argv) {
    var js = readJS(argv);
    var result = jsCompiler.createSnapshot(js, snapshotOptions(argv));
    bailOnError(result);
    writeOutput(argv, result.snapshot)
}

function patchPBW(js, inputBytes, argv) {
    var pbw = zip(inputBytes);

    // add or replace the JS in the PBW
    pbw.file('rocky-app.js', js);

    // this does the actual compiling (multiple times, yes, there's room for improvement)
    Object.getOwnPropertyNames(pbw.files).forEach(function(prop) {
        if (prop.endsWith('.pbpack')) {
            var pbpack = pbw.files[prop].asUint8Array();
            var result = jsCompiler.patchPBPack(js, pbpack, snapshotOptions(argv));
            if (result.result != 'success') {
                return result;
            }
            pbw.file(prop, new Buffer(result.pbpack));
        }
    });

    return {
        result: 'success',
        pbw: pbw.generate({type:'string', compression:'DEFLATE'})
    };
}

function patch(argv) {
    var js = readJS(argv);
    var inputBytes = strToByteArray(fs.readFileSync(argv.pbpack || argv.pbw, 'binary'));
    var result = argv.pbpack ? jsCompiler.patchPBPack(js, inputBytes, snapshotOptions(argv)) : patchPBW(js, inputBytes, argv);
    bailOnError(result);
    writeOutput(argv, result.pbpack || result.pbw);
}

function strToByteArray(str) {
    var result = new Array(str.length);
    for (var i = 0; i < str.length; i++) {
        result[i] = str[i].charCodeAt(0);
    }
    return result;
}

function byteArrayToStr(arr) {
    var result = '';
    for (i = 0; i < arr.length; i++) {
        result += String.fromCharCode(arr[i]);
    }
    return result;
}

function bailOnError(returnValue, prop) {
    if (returnValue.result != 'success') {
        console.error(returnValue.result + ':', returnValue.reason);
        process.exit(1);
    }
}

function writeOutput(argv, bytesOrString) {
    var data = typeof bytesOrString === 'string' ? bytesOrString : byteArrayToStr(bytesOrString);
    var isStdOut = argv.output == '/dev/stdout';
    if (isStdOut) {
        fs.writeSync(1, data, 'binary');
    } else {
        fs.writeFileSync(argv.output, data, 'binary');
    }
}

function readJS(argv) {
    return fs.readFileSync(argv.js, 'utf8');
}
