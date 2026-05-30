let engineInstance = null;
let cmdQueue = [];

var Module = {
    locateFile: function(path, prefix) {
        return prefix + path + '?v=5';
    },
    print: function(text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
        postMessage(text);
    },
    printErr: function(text) {
        if (arguments.length > 1) text = Array.prototype.slice.call(arguments).join(' ');
        console.error('Engine ERR:', text);
    },
    onRuntimeInitialized: function() {
        console.log('pawnGO Wasm Engine Initialized!');
        engineInstance = Module;
        
        // Execute any commands that were queued before initialization
        while (cmdQueue.length > 0) {
            let cmd = cmdQueue.shift();
            engineInstance.ccall('wasm_send_command', null, ['string'], [cmd]);
        }
        
        postMessage('isready');
    }
};

importScripts('pawngo_wasm.js?v=5');

self.addEventListener('message', function(e) {
    if (typeof e.data === 'string') {
        const cmd = e.data;
        if (engineInstance) {
            engineInstance.ccall('wasm_send_command', null, ['string'], [cmd]);
        } else {
            // Queue commands if engine is not yet ready (e.g. still downloading NNUE)
            cmdQueue.push(cmd);
        }
    }
});
