const HtmlWebpackPlugin = require('html-webpack-plugin');
const CopyPlugin = require("copy-webpack-plugin");
const HtmlMinimizerPlugin = require("html-minimizer-webpack-plugin");
const { CleanWebpackPlugin } = require("clean-webpack-plugin");
const MiniCssExtractPlugin = require("mini-css-extract-plugin");
const CompressionPlugin = require("compression-webpack-plugin");
const TerserPlugin = require("terser-webpack-plugin");
const CssMinimizerPlugin = require("css-minimizer-webpack-plugin");
const ImageMinimizerPlugin = require("image-minimizer-webpack-plugin");
const webpack = require("webpack");
const path = require("path");
const BundleAnalyzerPlugin = require('webpack-bundle-analyzer').BundleAnalyzerPlugin;
const globSync = require("glob").sync;
const glob = require('glob');
const { merge } = require('webpack-merge');
const devserver = require('./webpack/webpack.dev.js');
const fs = require('fs');
const zlib = require("zlib");

class BuildEventsHook {
  constructor(name, fn, stage = 'afterEmit') {
    this.name = name;
    this.stage = stage;
    this.function = fn;
  }
  apply(compiler) {
    compiler.hooks[this.stage].tap(this.name, this.function);
  }
}


module.exports = (env, options) => (
  merge(options.mode === "production" ? {} : devserver,
    {
      entry: 
      {
          index: './src/index.ts'
      },
      devtool: "source-map",
      module: {
        rules: [
          {
            test: /\.ejs$/,
            loader: 'ejs-loader',
            options: {
              variable: 'data',
              interpolate : '\\{\\{(.+?)\\}\\}',
              evaluate : '\\[\\[(.+?)\\]\\]'
            }
          },          
          {
            test: /\.(woff(2)?|ttf|eot)(\?v=\d+\.\d+\.\d+)?$/,
            use: [
              {
                loader: 'file-loader',
                options: {
                  name: '[name].[ext]',
                  outputPath: 'fonts/'
                }
              }
            ]
          }, 
          {
            test: /\.scss$|\.css$/,
            use: [
              // options.mode !== "production"
              //   ? "style-loader": 
                {
                  loader: MiniCssExtractPlugin.loader,
                  options: {
                    publicPath: "../",
                  },
                },
              "css-loader",
              {
                loader: "postcss-loader",
                options: {
                  postcssOptions: {
                    plugins: [["autoprefixer"]],
                  },
                },
              },
              "sass-loader",
            ],
          },
          {
            test: /\.js$/,
            exclude: /(node_modules|bower_components)/,
            use: {
              loader: "babel-loader",
              options: {
                presets: ["@babel/preset-env"],
              },
            },
          },
          {
            test: /\.(jpe?g|png|gif|svg)$/i,
            type: "asset",
          }, 
          // {
          //   test: /\.html$/i,
          //   type: "asset/resource",
          // },
        {
          test: /\.html$/i,
          loader: "html-loader",
          options: {
            minimize: true,
            
          }
        },
          //          {
          //   test: /\.html$/i,
          //   use: [
          //     "html-loader",
          //     {
          //       loader: 'markup-inline-loader',
          //       options: {
          //         svgo: {
          //           plugins: [
          //             {
          //               removeTitle: true,
          //             },
          //             {
          //               removeUselessStrokeAndFill: false,
          //             },
          //             {
          //               removeUnknownsAndDefaults: false,
          //             },
          //           ],
          //         },
          //       },
          //     },
          //   ]
          // },
          {
            test: /\.tsx?$/,
            use: 'ts-loader',
            exclude: /node_modules/,
          },
        ],
      },
      plugins: [
      
        new HtmlWebpackPlugin({
          title: 'SqueezeESP32',
          template: './src/index.ejs',
          filename: 'index.html',
          inject: 'body',
          minify: {
            html5                          : true,
            collapseWhitespace             : true,
            minifyCSS                      : true,
            minifyJS                       : true,
            minifyURLs                     : false,
            removeAttributeQuotes          : true,
            removeComments                 : true, // false for Vue SSR to find app placeholder
            removeEmptyAttributes          : true,
            removeOptionalTags             : true,
            removeRedundantAttributes      : true,
            removeScriptTypeAttributes     : true,
            removeStyleLinkTypeAttributese : true,
            useShortDoctype                : true
          },
          favicon: "./src/assets/images/favicon-32x32.png",
          excludeChunks: ['test'],
      }),
      // new CompressionPlugin({
      //   test: /\.(js|css|html|svg)$/,
      //   //filename: '[path].br[query]',
      //   filename: "[path][base].br",
      //   algorithm: 'brotliCompress',
      //   compressionOptions: { level: 11 },
      //   threshold: 100,
      //   minRatio: 0.8,
      //   deleteOriginalAssets: false
      // }),
      new CompressionPlugin({
          //filename: '[path].gz[query]',
          test: /\.js$|\.css$|\.html$/,
          filename: "[path][base].gz",

          algorithm: 'gzip',
          
          threshold: 100,
          minRatio: 0.8,
      }),
        new MiniCssExtractPlugin({
          filename: "css/[name].[contenthash].css",
        }),
        new webpack.ProvidePlugin({
          $: "jquery",
          jQuery: "jquery",
          "window.jQuery": "jquery",
          Popper: ["popper.js", "default"],
          Util: "exports-loader?Util!bootstrap/js/dist/util",
          Dropdown: "exports-loader?Dropdown!bootstrap/js/dist/dropdown",
        }),
        new BuildEventsHook('Update C App', 
          function (stats, arguments) {
            if (options.mode !== "production") return;
            fs.appendFileSync('./dist/index.html.gz', 
              zlib.gzipSync(fs.readFileSync('./dist/index.html'), 
              {
                chunckSize: 65536,
                level: zlib.constants.Z_BEST_COMPRESSION
             }));
            
            var getDirectories = function (src, callback) {
              var searchPath = path.posix.relative(process.cwd(), path.posix.join(__dirname, src, '/**/*(*.gz|favicon-32x32.png)'));
              console.log(`Post build: Getting file list from ${searchPath}`);
              glob(searchPath, callback);
            };
            var cleanUpPath = path.posix.relative(process.cwd(), path.posix.join(__dirname, '../../../build/*.S'));
            console.log(`Post build: Cleaning up previous builds in ${cleanUpPath}`);
            glob(cleanUpPath, function (err, list) {
              if (err) {
                console.error('Error', err);
              } else {
                list.forEach(fileName => {
                  try {
                    console.log(`Post build: Purging old binary file ${fileName} from C project.`);
                    fs.unlinkSync(fileName)
                    //file removed
                  } catch (ferr) {
                    console.error(ferr)
                  }
                });
              }
            },
            'afterEmit'
            );
            console.log('Generating C include files from webpack build output');
            getDirectories('./dist', function (err, list) {
              if (err) {
                console.log('Error', err);
              } else {
                const regex = /^(.*\/)([^\/]*)$/
                const relativeRegex = /((\w+(?<!dist)\/){0,1}[^\/]*)$/
                let exportDefHead =
`/***********************************
webpack_headers
${arguments[1]}
***********************************/
#pragma once
#include <inttypes.h>
extern const char * resource_lookups[];
extern const uint8_t * resource_map_start[];
extern const uint8_t * resource_map_end[];`;
                let exportDef = '// Automatically generated. Do not edit manually!.\n' +
                  '#include <inttypes.h>\n';
                let lookupDef = 'const char * resource_lookups[] = {\n';
                let lookupMapStart = 'const uint8_t * resource_map_start[] = {\n';
                let lookupMapEnd = 'const uint8_t * resource_map_end[] = {\n';
                let cMake = '';
                list.push('./dist/index.html.gz');
                list.forEach(fileName => {

                  let exportName = fileName.match(regex)[2].replace(/[\. \-]/gm, '_');
                  let relativeName = fileName.match(relativeRegex)[1];
                  exportDef += `extern const uint8_t _${exportName}_start[] asm("_binary_${exportName}_start");\nextern const uint8_t _${exportName}_end[] asm("_binary_${exportName}_end");\n`;
                  lookupDef += '\t"/' + relativeName + '",\n';
                  lookupMapStart += '\t_' + exportName + '_start,\n';
                  lookupMapEnd += '\t_' + exportName + '_end,\n';
                  cMake += `target_add_binary_data( __idf_wifi-manager ${path.posix.relative(path.posix.resolve(process.cwd(),'..','..'),fileName)
                } BINARY)\n`;
                });

                lookupDef += '""\n};\n';
                lookupMapStart = lookupMapStart.substring(0, lookupMapStart.length - 2) + '\n};\n';
                lookupMapEnd = lookupMapEnd.substring(0, lookupMapEnd.length - 2) + '\n};\n';
                try {
                  fs.writeFileSync('webapp.cmake', cMake);
                  fs.writeFileSync('webpack.c', exportDef + lookupDef + lookupMapStart + lookupMapEnd);
                  fs.writeFileSync('webpack.h', exportDefHead);
                  //file written successfully
                } catch (e) {
                  console.error(e);
                }
              }
            });
            console.log('Post build completed.');

          })
      ],
      optimization: {
        minimize: true,
        minimizer: [
         
          new TerserPlugin(),
          new HtmlMinimizerPlugin(),
          new CssMinimizerPlugin(),
          new ImageMinimizerPlugin({
            minimizer: {
              implementation: ImageMinimizerPlugin.imageminMinify,
              options: {
                // Lossless optimization with custom option
                // Feel free to experiment with options for better result for you
                plugins: [
                  ["gifsicle", { interlaced: true }],
                  ["jpegtran", { progressive: true }],
                  ["optipng", { optimizationLevel: 5 }],
                  // Svgo configuration here https://github.com/svg/svgo#configuration
                  [
                    "svgo",
                    {
                      plugins: [
                       {
                          name: 'preset-default',
                         params: {
                          overrides: {
                            // customize default plugin options
                            inlineStyles: {
                              onlyMatchedOnce: false,
                            },
                  
                            // or disable plugins
                            removeDoctype: false,
                          },
                        },
                      }
                      ],
                    },
                  ],
                ],
              },
            },
          }),  
          //new BundleAnalyzerPlugin()

        ],
        // runtimeChunk: 'single',
        // splitChunks: {
        //     chunks: 'all',
        //     // maxInitialRequests: Infinity,
        //     // minSize: 0,
        //     cacheGroups: {
        //         vendor: {
        //             test: /node_modules/, // you may add "vendor.js" here if you want to
        //             name: "node-modules",
        //             chunks: "initial",
        //             enforce: true
        //         },
        //     }
        //   }
      },
      //   output: {
      //     filename: "[name].js",
      //     path: path.resolve(__dirname, "dist"),
      //     publicPath: "",
      //   },
      resolve: {
        extensions: ['.tsx', '.ts', '.js', '.ejs' ],
      },
      output: {
        path: path.resolve(__dirname, 'dist'),
        filename: './js/[name].[fullhash:6].bundle.js',
        clean: true
      },
    }
  )
);