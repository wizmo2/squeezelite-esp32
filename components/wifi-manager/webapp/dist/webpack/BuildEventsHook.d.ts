export class BuildEventsHook {
    constructor(name: any, fn: any, stage?: string);
    name: any;
    stage: string;
    function: any;
    apply(compiler: any): void;
}
export function createBuildEventsHook(options: any): BuildEventsHook;
