/// <reference lib="webworker" />
export namespace languages {
    export namespace markup {
        namespace tag { }
    }
    import html = markup;
    export { html };
    import mathml = markup;
    export { mathml };
    import svg = markup;
    export { svg };
    export const xml: {
        [x: string]: RegExp | GrammarToken | (RegExp | GrammarToken)[];
    };
    import ssml = xml;
    export { ssml };
    import atom = xml;
    export { atom };
    import rss = xml;
    export { rss };
    export const clike: {
        comment: {
            pattern: RegExp;
            lookbehind: boolean;
            greedy: boolean;
        }[];
        string: {
            pattern: RegExp;
            greedy: boolean;
        };
        'class-name': {
            pattern: RegExp;
            lookbehind: boolean;
            inside: {
                punctuation: RegExp;
            };
        };
        keyword: RegExp;
        boolean: RegExp;
        function: RegExp;
        number: RegExp;
        operator: RegExp;
        punctuation: RegExp;
    };
    export const javascript: {
        [x: string]: RegExp | GrammarToken | (RegExp | GrammarToken)[];
    };
    import js = javascript;
    export { js };
}
/**
 * The expansion of a simple `RegExp` literal to support additional properties.
 */
export type GrammarToken = {
    /**
     * The regular expression of the token.
     */
    pattern: RegExp;
    /**
     * If `true`, then the first capturing group of `pattern` will (effectively)
     * behave as a lookbehind group meaning that the captured text will not be part of the matched text of the new token.
     */
    lookbehind?: boolean;
    /**
     * Whether the token is greedy.
     */
    greedy?: boolean;
    /**
     * An optional alias or list of aliases.
     */
    alias?: string | string[];
    /**
     * The nested grammar of this token.
     *
     * The `inside` grammar will be used to tokenize the text value of each token of this kind.
     *
     * This can be used to make nested and even recursive language definitions.
     *
     * Note: This can cause infinite recursion. Be careful when you embed different languages or even the same language into
     * each another.
     */
    inside?: Grammar;
};
export type Grammar = {
    [x: string]: RegExp | GrammarToken | Array<RegExp | GrammarToken>;
};
/**
 * A function which will invoked after an element was successfully highlighted.
 */
export type HighlightCallback = (element: Element) => void;
export type HookCallback = (env: {
    [x: string]: any;
}) => void;
