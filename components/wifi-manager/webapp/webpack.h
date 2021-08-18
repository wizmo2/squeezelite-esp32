/***********************************
webpack_headers
Hash: 0e064eadc7c8b7881633
Version: webpack 4.46.0
Time: 9582ms
Built at: 2021-07-09 11 h 14 min 41 s
                                Asset       Size  Chunks                                Chunk Names
          ./js/index.0e064e.bundle.js    232 KiB       0  [emitted] [immutable]         index
       ./js/index.0e064e.bundle.js.br   32.7 KiB          [emitted]                     
       ./js/index.0e064e.bundle.js.gz     42 KiB          [emitted]                     
   ./js/node-modules.0e064e.bundle.js    266 KiB       1  [emitted] [immutable]  [big]  node-modules
./js/node-modules.0e064e.bundle.js.br   76.3 KiB          [emitted]                     
./js/node-modules.0e064e.bundle.js.gz   88.7 KiB          [emitted]                     
        ./js/runtime.0e064e.bundle.js   1.46 KiB       2  [emitted] [immutable]         runtime
     ./js/runtime.0e064e.bundle.js.br  644 bytes          [emitted]                     
     ./js/runtime.0e064e.bundle.js.gz  722 bytes          [emitted]                     
                    favicon-32x32.png  634 bytes          [emitted]                     
                           index.html   21.7 KiB          [emitted]                     
                        index.html.br   4.74 KiB          [emitted]                     
                        index.html.gz   5.75 KiB          [emitted]                     
                           sprite.svg    4.4 KiB          [emitted]                     
                        sprite.svg.br  898 bytes          [emitted]                     
Entrypoint index [big] = ./js/runtime.0e064e.bundle.js ./js/node-modules.0e064e.bundle.js ./js/index.0e064e.bundle.js
 [6] ./node_modules/bootstrap/dist/js/bootstrap-exposed.js 437 bytes {1} [built]
[11] ./src/sass/main.scss 1.55 KiB {0} [built]
[16] ./node_modules/remixicon/icons/Device/signal-wifi-fill.svg 340 bytes {1} [built]
[17] ./node_modules/remixicon/icons/Device/signal-wifi-3-fill.svg 344 bytes {1} [built]
[18] ./node_modules/remixicon/icons/Device/signal-wifi-2-fill.svg 344 bytes {1} [built]
[19] ./node_modules/remixicon/icons/Device/signal-wifi-1-fill.svg 344 bytes {1} [built]
[20] ./node_modules/remixicon/icons/Device/signal-wifi-line.svg 340 bytes {1} [built]
[21] ./node_modules/remixicon/icons/Device/battery-line.svg 332 bytes {1} [built]
[22] ./node_modules/remixicon/icons/Device/battery-low-line.svg 340 bytes {1} [built]
[23] ./node_modules/remixicon/icons/Device/battery-fill.svg 332 bytes {1} [built]
[24] ./node_modules/remixicon/icons/Media/headphone-fill.svg 335 bytes {1} [built]
[25] ./node_modules/remixicon/icons/Device/device-recover-fill.svg 346 bytes {1} [built]
[26] ./node_modules/remixicon/icons/Device/bluetooth-fill.svg 336 bytes {1} [built]
[27] ./node_modules/remixicon/icons/Device/bluetooth-connect-fill.svg 352 bytes {1} [built]
[38] ./src/index.ts + 1 modules 62.6 KiB {0} [built]
     | ./src/index.ts 1.4 KiB [built]
     | ./src/js/custom.js 61 KiB [built]
    + 24 hidden modules

WARNING in asset size limit: The following asset(s) exceed the recommended size limit (244 KiB).
This can impact web performance.
Assets: 
  ./js/node-modules.0e064e.bundle.js (266 KiB)

WARNING in entrypoint size limit: The following entrypoint(s) combined asset size exceeds the recommended limit (244 KiB). This can impact web performance.
Entrypoints:
  index (499 KiB)
      ./js/runtime.0e064e.bundle.js
      ./js/node-modules.0e064e.bundle.js
      ./js/index.0e064e.bundle.js


WARNING in webpack performance recommendations: 
You can limit the size of your bundles by using import() or require.ensure to lazy load some parts of your application.
For more info visit https://webpack.js.org/guides/code-splitting/
Child html-webpack-plugin for "index.html":
         Asset     Size  Chunks  Chunk Names
    index.html  560 KiB       0  
    Entrypoint undefined = index.html
    [0] ./node_modules/html-webpack-plugin/lib/loader.js!./src/index.ejs 23.9 KiB {0} [built]
    [1] ./node_modules/lodash/lodash.js 531 KiB {0} [built]
    [2] (webpack)/buildin/global.js 472 bytes {0} [built]
    [3] (webpack)/buildin/module.js 497 bytes {0} [built]
***********************************/
#pragma once
#include <inttypes.h>
extern const char * resource_lookups[];
extern const uint8_t * resource_map_start[];
extern const uint8_t * resource_map_end[];
