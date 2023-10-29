declare const PORT: 5000;
import HtmlWebPackPlugin = require("html-webpack-plugin");
export namespace entry {
    const test: string;
}
export namespace devServer {
    export namespace _static {
        const directory: string;
        const staticOptions: {};
        const publicPath: string;
        const serveIndex: boolean;
        const watch: boolean;
    }
    export { _static as static };
    export namespace devMiddleware {
        const publicPath_1: string;
        export { publicPath_1 as publicPath };
    }
    export const open: boolean;
    export const compress: boolean;
    export { PORT as port };
    export const host: string;
    export const allowedHosts: string;
    export const headers: {
        'Access-Control-Allow-Origin': string;
        'Accept-Encoding': string;
    };
    export namespace client {
        const logging: string;
        const overlay: boolean;
        const progress: boolean;
    }
    export function onListening(devServer: any): void;
    export function onBeforeSetupMiddleware(devServer: any): void;
}
export const plugins: HtmlWebPackPlugin[];
export {};
