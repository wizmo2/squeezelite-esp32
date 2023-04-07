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
const PurgeCSSPlugin = require('purgecss-webpack-plugin')
const whitelister = require('purgecss-whitelister');


const PATHS = {
  src: path.join(__dirname, 'src')
}

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
  merge(
    env.WEBPACK_SERVE ?  devserver : {},
    env.ANALYZE_SIZE?{ plugins: [ new BundleAnalyzerPlugin(
      {
        analyzerMode: 'static',
        generateStatsFile: true,
        statsFilename: 'stats.json',
      }
    )      ]}:{},

    
    {
      entry: 
      {
          index: './src/index.ts'
      },
      devtool:"source-map",
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
          // {
          //   test: /\.s[ac]ss$/i,
            
          //   use:  [{
          //     loader: 'style-loader', // inject CSS to page
          //   },
          //      {
          //         loader: MiniCssExtractPlugin.loader,
          //         options: {
          //           publicPath: "../",
          //         },
          //       },
          //     "css-loader",
          //     {
          //       loader: "postcss-loader",
          //       options: {
          //         postcssOptions: {
          //           plugins: [["autoprefixer"]],
          //         },
          //       },
          //     },
          //     "sass-loader",
          //   ]
          // },
          {
            test: /\.(scss)$/,
            use: [
              
              {
                loader: MiniCssExtractPlugin.loader,
                options: {
                  publicPath: "../",
                },
              },
            //   {
            //   // inject CSS to page
            //   loader: 'style-loader'
            // }, 
            {
              // translates CSS into CommonJS modules
              loader: 'css-loader'
            },
                 
           {
              // Run postcss actions
              loader: 'postcss-loader',
              options: {
                // `postcssOptions` is needed for postcss 8.x;
                // if you use postcss 7.x skip the key
                postcssOptions: {
                  // postcss plugins, can be exported to postcss.config.js
                  plugins: function () {
                    return [
                      require('autoprefixer')
                    ];
                  }
                }
              }
            }, {
              // compiles Sass to CSS
              loader: 'sass-loader'
            }]
          },          
          {
            test: /\.js$/,
            exclude: /(node_modules|bower_components)/,
            use: {
              loader: "babel-loader",
              options: {
                presets: ["@babel/preset-env"],
                plugins: ['@babel/plugin-transform-runtime']

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
        new MiniCssExtractPlugin({
          filename: "css/[name].[contenthash].css",
        }),
        new PurgeCSSPlugin({
          keyframes: false,
          paths: glob.sync(`${path.join(__dirname, 'src')}/**/*`, {
            nodir: true
          }),
          whitelist: whitelister('bootstrap/dist/css/bootstrap.css')
        }),
        new webpack.ProvidePlugin({
          $: "jquery",
//          jQuery: "jquery",
  //        "window.jQuery": "jquery",
    //      Popper: ["popper.js", "default"],
          // Util: "exports-loader?Util!bootstrap/js/dist/util",
          // Dropdown: "exports-loader?Dropdown!bootstrap/js/dist/dropdown",
        }),
        new CompressionPlugin({
          //filename: '[path].gz[query]',
          test: /\.js$|\.css$|\.html$/,
          filename: "[path][base].gz",

          algorithm: 'gzip',
          
          threshold: 100,
          minRatio: 0.8,
      }),
      
        new BuildEventsHook('Update C App', 
          function (stats, arguments) {

            if (options.mode !== "production") return;
            let buildRootPath = path.join(process.cwd(),'..','..','..');
            let wifiManagerPath=glob.sync(path.join(buildRootPath,'components/**/wifi-manager*'))[0];
            let buildCRootPath=glob.sync(buildRootPath)[0];
            fs.appendFileSync('./dist/index.html.gz', 
              zlib.gzipSync(fs.readFileSync('./dist/index.html'), 
              {
                chunckSize: 65536,
                level: zlib.constants.Z_BEST_COMPRESSION
             }));
            
            var getDirectories = function (src, callback) {
              var searchPath = path.posix.join(src, '/**/*(*.gz|favicon-32x32.png)');
              console.log(`Post build: Getting file list from ${searchPath}`);
              glob(searchPath, callback);
            };
            var cleanUpPath = path.posix.join(buildCRootPath, '/build/*.S');
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
              console.log(`Post build: found ${list.length} files. Relative path: ${wifiManagerPath}.`);
              if (err) {
                console.log('Error', err);
              } else {
              
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
                let cMake='';
                
                list.forEach(foundFile => {
                  let exportName = path.basename(foundFile).replace(/[\. \-]/gm, '_');
                  //take the full path of the file and make it relative to the build directory
                  let cmakeFileName = path.posix.relative(wifiManagerPath,glob.sync(path.resolve(foundFile))[0]);
                  let httpRelativePath=path.posix.join('/',path.posix.relative('dist',foundFile));
                  exportDef += `extern const uint8_t _${exportName}_start[] asm("_binary_${exportName}_start");\nextern const uint8_t _${exportName}_end[] asm("_binary_${exportName}_end");\n`;
                  lookupDef += `\t"${httpRelativePath}",\n`;
                  lookupMapStart += '\t_' + exportName + '_start,\n';
                  lookupMapEnd += '\t_' + exportName + '_end,\n';
                  cMake += `target_add_binary_data( __idf_wifi-manager ${cmakeFileName} BINARY)\n`;
                  console.log(`Post build: adding cmake file reference to ${cmakeFileName} from C project, with web path ${httpRelativePath}.`);
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
        providedExports: true,
        usedExports: true,
        minimizer: [
         
          new TerserPlugin({
            terserOptions: {
              format: {
                  comments: false,
              },
            },
            extractComments: false,
          // enable parallel running
            parallel: true,
          }),
          new HtmlMinimizerPlugin({
            minimizerOptions: {
              removeComments: true,
              removeOptionalTags: true,

            }
          }
          ),
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
          

        ],
        splitChunks: {
          cacheGroups: {
              vendor: {
                  name: "node_vendors",
                  test: /[\\/]node_modules[\\/]/,
                  chunks: "all",
              }
          }
      }
      
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