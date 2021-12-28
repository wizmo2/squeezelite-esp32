/* eslint-disable  */
// Merges webpack.common config with this production config
const merge = require('webpack-merge');
const common = require('./webpack.common.js');

const webpack = require('webpack');
const CleanWebpackPlugin = require('clean-webpack-plugin');

// Optimisations and Compression
const MiniCssExtractPlugin = require('mini-css-extract-plugin');
const TerserPlugin = require('terser-webpack-plugin');
const OptimizeCSSAssetsPlugin = require('optimize-css-assets-webpack-plugin');
const CompressionPlugin = require('compression-webpack-plugin');

const fs = require('fs');
const glob = require('glob');
var WebpackOnBuildPlugin = require('on-build-webpack');
const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;
const path = require('path')
const ExtractTextPlugin = require('extract-text-webpack-plugin')
const PurgecssPlugin = require('purgecss-webpack-plugin')

// Optional
const FaviconsWebpackPlugin = require('favicons-webpack-plugin');
const PATHS = {
    src: path.join(__dirname, 'src')
  }
module.exports = merge(common, {
    mode: 'production',
    stats: 'errors-only',
    optimization: {
        minimizer: [
            new TerserPlugin({
                test: /\.js(\?.*)?$/i,
                exclude: /node_modules/,
                cache: true,
                parallel: 4,
                sourceMap: true,
            }),
            new OptimizeCSSAssetsPlugin({})
        ],
        runtimeChunk: 'single',
        splitChunks: {
            chunks: 'all',
            // maxInitialRequests: Infinity,
            // minSize: 0,
            cacheGroups: {
                vendor: {
                    test: /node_modules/, // you may add "vendor.js" here if you want to
                    name: "node-modules",
                    chunks: "initial",
                    enforce: true
                },
            }
        },
    },
    plugins: [
        new MiniCssExtractPlugin({
            filename: 'css/[name].[hash:6].css',
            chunkFilename: 'css/[name].[contenthash].css',

        }),
        new ExtractTextPlugin('[name].css?[hash]'),
    new PurgecssPlugin({
      paths: glob.sync(`${PATHS.src}/*`),
      whitelist: ['whitelisted']
    }),
        new CleanWebpackPlugin(),
        new CompressionPlugin({
            test: /\.(js|css|html|svg)$/,
            filename: '[path].br[query]',
            algorithm: 'brotliCompress',
            compressionOptions: { level: 11 },
            threshold: 100,
            minRatio: 0.8,
            deleteOriginalAssets: false
        }),
        new CompressionPlugin({
            filename: '[path].gz[query]',
            algorithm: 'gzip',
            test: /\.js$|\.css$|\.html$/,
            threshold: 100,
            minRatio: 0.8,
        }),

        // new FaviconsWebpackPlugin({
        //     // Your source logo
        //     logo: './src/assets/images/200px-ControllerAppIcon.png',
        //     // // The prefix for all image files (might be a folder or a name)
        //     //prefix: 'assets/icons_[hash:6]/',
        //     prefix: 'icons_[hash:6]/',
        //     // // Emit all stats of the generated icons
        //     //emitStats: false,
        //     // // The name of the json containing all favicon information
        //     // statsFilename: 'iconstats-[hash].json',
        //     // // Generate a cache file with control hashes and
        //     // // don't rebuild the favicons until those hashes change
        //     persistentCache: true,
        //     // // Inject the html into the html-webpack-plugin
        //     inject: true,
        //     // // favicon background color (see https://github.com/haydenbleasel/favicons#usage)
        //     background: '#fff',
        //     // // which icons should be generated (see https://github.com/haydenbleasel/favicons#usage)
        //      icons: {
        //     //   android: false,
        //     //   appleIcon: false,
        //        favicons: true
        //     //   firefox: true,
        //     //   windows: false
        //     }
        // }),
        new WebpackOnBuildPlugin(function(stats) {

            var getDirectories = function (src, callback) {
                glob(`${src}/**/*(*.gz|favicon-32x32.png)`, callback);
                };
            console.log('Cleaning up previous builds');
            glob(`../../../build/*.S`, function (err, list) {                  
                if (err) {
                    console.error('Error', err);
                } else {
                    list.forEach(fileName=>{
                        try {
                            console.log(`Purging old binary file ${fileName} from C project.`);
                            fs.unlinkSync(fileName)
                            //file removed
                            } catch(ferr) {
                            console.error(ferr)
                            }
                    });
                }
            }
            );
            console.log('Generating C include files from webpack build output');
            getDirectories('./webpack/dist', function (err, list) {
                if (err) {
                    console.log('Error', err);
                } else {
                    const regex = /^(.*\/)([^\/]*)$/
                    const relativeRegex = /((\w+(?<!dist)\/){0,1}[^\/]*)$/
                    const makePathRegex = /([^\.].*)$/
                    let exportDefHead=
                    '/***********************************\n'+
                    'webpack_headers\n'+
                    stats+'\n'+
                    '***********************************/\n'+
                    '#pragma once\n'+
                    '#include <inttypes.h>\n'+
                    'extern const char * resource_lookups[];\n'+
                    'extern const uint8_t * resource_map_start[];\n'+
                    'extern const uint8_t * resource_map_end[];\n';
                    let exportDef=  '// Automatically generated. Do not edit manually!.\n'+
                                    '#include <inttypes.h>\n';
                    let lookupDef='const char * resource_lookups[] = {\n';
                    let lookupMapStart='const uint8_t * resource_map_start[] = {\n';
                    let lookupMapEnd='const uint8_t * resource_map_end[] = {\n';
                    let cMake='';
                    list.forEach(fileName=>{
                            let exportName=fileName.match(regex)[2].replace(/[\. \-]/gm,'_');
                            let relativeName=fileName.match(relativeRegex)[1];
                            exportDef+=	'extern const uint8_t _'+exportName+'_start[] asm("_binary_'+exportName+'_start");\n'+
                                    'extern const uint8_t _'+exportName+'_end[] asm("_binary_'+exportName+'_end");\n';
                            lookupDef+='\t"/'+relativeName+'",\n';
                            lookupMapStart+='\t_'+ exportName+'_start,\n';
                            lookupMapEnd+= '\t_'+ exportName+'_end,\n';
                            cMake+='target_add_binary_data( __idf_wifi-manager ./webapp'+fileName.match(makePathRegex)[1]+' BINARY)\n';
                    });

                    lookupDef+='""\n};\n';
                    lookupMapStart=lookupMapStart.substring(0,lookupMapStart.length-2)+'\n};\n';
                    lookupMapEnd=lookupMapEnd.substring(0,lookupMapEnd.length-2)+'\n};\n';
                    try {
                        fs.writeFileSync('webapp.cmake', cMake);
                        fs.writeFileSync('webpack.c', exportDef+lookupDef+lookupMapStart+lookupMapEnd);
                        fs.writeFileSync('webpack.h', exportDefHead);
                        //file written successfully
                        } catch (e) {
                        console.error(e);
                        }        
                }
            });
            console.log('Post build completed.');

        })
        //,        new BundleAnalyzerPlugin()               
    ]
});

