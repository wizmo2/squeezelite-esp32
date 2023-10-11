declare const Alert_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Alert extends Alert_base {
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    close(): void;
    _destroyElement(): void;
}
declare const Button_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Button extends Button_base {
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    toggle(): void;
}
declare const Carousel_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Carousel extends Carousel_base {
    static get Default(): {
        interval: number;
        keyboard: boolean;
        pause: string;
        ride: boolean;
        touch: boolean;
        wrap: boolean;
    };
    static get DefaultType(): {
        interval: string;
        keyboard: string;
        pause: string;
        ride: string;
        touch: string;
        wrap: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    _interval: NodeJS.Timeout;
    _activeElement: any;
    _isSliding: boolean;
    touchTimeout: NodeJS.Timeout;
    _swipeHelper: {
        _element: any;
        _config: any;
        _deltaX: number;
        _supportPointerEvents: boolean;
        dispose(): void;
        _start(event: any): void;
        _end(event: any): void;
        _move(event: any): void;
        _handleSwipe(): void;
        _initEvents(): void;
        _eventIsPointerPenTouch(event: any): boolean;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _indicatorsElement: any;
    next(): void;
    nextWhenVisible(): void;
    prev(): void;
    pause(): void;
    cycle(): void;
    _maybeEnableCycle(): void;
    to(index: any): void;
    _addEventListeners(): void;
    _addTouchEventListeners(): void;
    _keydown(event: any): void;
    _getItemIndex(element: any): number;
    _setActiveIndicatorElement(index: any): void;
    _updateInterval(): void;
    _slide(order: any, element?: any): void;
    _isAnimated(): any;
    _getActive(): any;
    _getItems(): any[];
    _clearInterval(): void;
    _directionToOrder(direction: any): "next" | "prev";
    _orderToDirection(order: any): "right" | "left";
}
declare const Collapse_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Collapse extends Collapse_base {
    static get Default(): {
        parent: any;
        toggle: boolean;
    };
    static get DefaultType(): {
        parent: string;
        toggle: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    _isTransitioning: boolean;
    _triggerArray: any[];
    toggle(): void;
    show(): void;
    hide(): void;
    _isShown(element?: any): any;
    _getDimension(): "width" | "height";
    _initializeChildren(): void;
    _getFirstLevelChildren(selector: any): any[];
    _addAriaAndCollapsedClass(triggerArray: any, isOpen: any): void;
}
declare const Dropdown_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Dropdown extends Dropdown_base {
    static get Default(): {
        autoClose: boolean;
        boundary: string;
        display: string;
        offset: number[];
        popperConfig: any;
        reference: string;
    };
    static get DefaultType(): {
        autoClose: string;
        boundary: string;
        display: string;
        offset: string;
        popperConfig: string;
        reference: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    static clearMenus(event: any): void;
    static dataApiKeydownHandler(event: any): void;
    _popper: any;
    _parent: any;
    _menu: any;
    _inNavbar: boolean;
    toggle(): void;
    show(): void;
    hide(): void;
    update(): void;
    _completeHide(relatedTarget: any): void;
    _createPopper(): void;
    _isShown(): any;
    _getPlacement(): "top" | "bottom" | "top-end" | "top-start" | "bottom-end" | "bottom-start" | "left-start" | "right-start";
    _detectNavbar(): boolean;
    _getOffset(): any;
    _getPopperConfig(): any;
    _selectMenuItem({ key, target }: {
        key: any;
        target: any;
    }): void;
}
declare const Modal_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Modal extends Modal_base {
    static get Default(): {
        backdrop: boolean;
        focus: boolean;
        keyboard: boolean;
    };
    static get DefaultType(): {
        backdrop: string;
        focus: string;
        keyboard: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any, relatedTarget: any): any;
    _dialog: any;
    _backdrop: {
        _config: any;
        _isAppended: boolean;
        _element: HTMLDivElement;
        show(callback: any): void;
        hide(callback: any): void;
        dispose(): void;
        _getElement(): HTMLDivElement;
        _configAfterMerge(config: any): any;
        _append(): void;
        _emulateAnimation(callback: any): void;
        _getConfig(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _focustrap: {
        _config: any;
        _isActive: boolean;
        _lastTabNavDirection: string;
        activate(): void;
        deactivate(): void;
        _handleFocusin(event: any): void;
        _handleKeydown(event: any): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _isShown: boolean;
    _isTransitioning: boolean;
    _scrollBar: {
        _element: HTMLElement;
        getWidth(): number;
        hide(): void;
        reset(): void;
        isOverflowing(): boolean;
        _disableOverFlow(): void;
        _setElementAttributes(selector: any, styleProperty: any, callback: any): void;
        _saveInitialAttribute(element: any, styleProperty: any): void;
        _resetElementAttributes(selector: any, styleProperty: any): void;
        _applyManipulationCallback(selector: any, callBack: any): void;
    };
    toggle(relatedTarget: any): void;
    show(relatedTarget: any): void;
    hide(): void;
    handleUpdate(): void;
    _initializeBackDrop(): {
        _config: any;
        _isAppended: boolean;
        _element: HTMLDivElement;
        show(callback: any): void;
        hide(callback: any): void;
        dispose(): void;
        _getElement(): HTMLDivElement;
        _configAfterMerge(config: any): any;
        _append(): void;
        _emulateAnimation(callback: any): void;
        _getConfig(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _initializeFocusTrap(): {
        _config: any;
        _isActive: boolean;
        _lastTabNavDirection: string;
        activate(): void;
        deactivate(): void;
        _handleFocusin(event: any): void;
        _handleKeydown(event: any): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _showElement(relatedTarget: any): void;
    _addEventListeners(): void;
    _hideModal(): void;
    _isAnimated(): any;
    _triggerBackdropTransition(): void;
    /**
     * The following methods are used to handle overflowing modals
     */
    _adjustDialog(): void;
    _resetAdjustments(): void;
}
declare const Offcanvas_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Offcanvas extends Offcanvas_base {
    static get Default(): {
        backdrop: boolean;
        keyboard: boolean;
        scroll: boolean;
    };
    static get DefaultType(): {
        backdrop: string;
        keyboard: string;
        scroll: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    _isShown: boolean;
    _backdrop: {
        _config: any;
        _isAppended: boolean;
        _element: HTMLDivElement;
        show(callback: any): void;
        hide(callback: any): void;
        dispose(): void;
        _getElement(): HTMLDivElement;
        _configAfterMerge(config: any): any;
        _append(): void;
        _emulateAnimation(callback: any): void;
        _getConfig(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _focustrap: {
        _config: any;
        _isActive: boolean;
        _lastTabNavDirection: string;
        activate(): void;
        deactivate(): void;
        _handleFocusin(event: any): void;
        _handleKeydown(event: any): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    toggle(relatedTarget: any): void;
    show(relatedTarget: any): void;
    hide(): void;
    _initializeBackDrop(): {
        _config: any;
        _isAppended: boolean;
        _element: HTMLDivElement;
        show(callback: any): void;
        hide(callback: any): void;
        dispose(): void;
        _getElement(): HTMLDivElement;
        _configAfterMerge(config: any): any;
        _append(): void;
        _emulateAnimation(callback: any): void;
        _getConfig(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _initializeFocusTrap(): {
        _config: any;
        _isActive: boolean;
        _lastTabNavDirection: string;
        activate(): void;
        deactivate(): void;
        _handleFocusin(event: any): void;
        _handleKeydown(event: any): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    _addEventListeners(): void;
}
declare const Popover_base: {
    new (element: any, config: any): {
        _isEnabled: boolean;
        _timeout: number;
        _isHovered: boolean;
        _activeTrigger: {};
        _popper: any;
        _templateFactory: {
            _config: any;
            getContent(): any[];
            hasContent(): boolean;
            changeContent(content: any): any;
            toHtml(): Element;
            _typeCheckConfig(config: any): void;
            _checkContent(arg: any): void;
            _setContent(template: any, content: any, selector: any): void;
            _maybeSanitize(arg: any): any;
            _resolvePossibleFunction(arg: any): any;
            _putElementInTemplate(element: any, templateElement: any): void;
            _getConfig(config: any): any;
            _configAfterMerge(config: any): any;
            _mergeConfigObj(config: any, element: any): any;
        };
        _newContent: any;
        tip: Element;
        enable(): void;
        disable(): void;
        toggleEnabled(): void;
        toggle(): void;
        dispose(): void;
        show(): void;
        hide(): void;
        update(): void;
        _isWithContent(): boolean;
        _getTipElement(): Element;
        _createTipElement(content: any): Element;
        setContent(content: any): void;
        _getTemplateFactory(content: any): {
            _config: any;
            getContent(): any[];
            hasContent(): boolean;
            changeContent(content: any): any;
            toHtml(): Element;
            _typeCheckConfig(config: any): void;
            _checkContent(arg: any): void;
            _setContent(template: any, content: any, selector: any): void;
            _maybeSanitize(arg: any): any;
            _resolvePossibleFunction(arg: any): any;
            _putElementInTemplate(element: any, templateElement: any): void;
            _getConfig(config: any): any;
            _configAfterMerge(config: any): any;
            _mergeConfigObj(config: any, element: any): any;
        };
        _getContentForTemplate(): {
            ".tooltip-inner": any;
        };
        _getTitle(): any;
        _initializeOnDelegatedTarget(event: any): any;
        _isAnimated(): any;
        _isShown(): boolean;
        _createPopper(tip: any): any;
        _getOffset(): any;
        _resolvePossibleFunction(arg: any): any;
        _getPopperConfig(attachment: any): any;
        _setListeners(): void;
        _hideModalHandler: () => void;
        _fixTitle(): void;
        _enter(): void;
        _leave(): void;
        _setTimeout(handler: any, timeout: any): void;
        _isWithActiveTrigger(): boolean;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _getDelegateConfig(): {
            selector: boolean;
            trigger: string;
        };
        _disposePopper(): void;
        _element: any;
        _config: any;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    readonly Default: {
        allowList: {
            '*': (string | RegExp)[];
            a: string[];
            area: any[];
            b: any[];
            br: any[];
            col: any[];
            code: any[];
            div: any[];
            em: any[];
            hr: any[];
            h1: any[];
            h2: any[];
            h3: any[];
            h4: any[];
            h5: any[];
            h6: any[];
            i: any[];
            img: string[];
            li: any[];
            ol: any[];
            p: any[];
            pre: any[];
            s: any[];
            small: any[];
            span: any[];
            sub: any[];
            sup: any[];
            strong: any[];
            u: any[];
            ul: any[];
        };
        animation: boolean;
        boundary: string;
        container: boolean;
        customClass: string;
        delay: number;
        fallbackPlacements: string[];
        html: boolean;
        offset: number[];
        placement: string;
        popperConfig: any;
        sanitize: boolean;
        sanitizeFn: any;
        selector: boolean;
        template: string;
        title: string;
        trigger: string;
    };
    readonly DefaultType: {
        allowList: string;
        animation: string;
        boundary: string;
        container: string;
        customClass: string;
        delay: string;
        fallbackPlacements: string;
        html: string;
        offset: string;
        placement: string;
        popperConfig: string;
        sanitize: string;
        sanitizeFn: string;
        selector: string;
        template: string;
        title: string;
        trigger: string;
    };
    readonly NAME: string;
    jQueryInterface(config: any): any;
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
};
/**
 * Class definition
 */
export class Popover extends Popover_base {
    static get Default(): {
        content: string;
        offset: number[];
        placement: string;
        template: string;
        trigger: string;
        allowList: {
            '*': (string | RegExp)[];
            a: string[];
            area: any[];
            b: any[];
            br: any[];
            col: any[];
            code: any[];
            div: any[];
            em: any[];
            hr: any[];
            h1: any[];
            h2: any[];
            h3: any[];
            h4: any[];
            h5: any[];
            h6: any[];
            i: any[];
            img: string[];
            li: any[];
            ol: any[];
            p: any[];
            pre: any[];
            s: any[];
            small: any[];
            span: any[];
            sub: any[];
            sup: any[];
            strong: any[];
            u: any[];
            ul: any[];
        };
        animation: boolean;
        boundary: string;
        container: boolean;
        customClass: string;
        delay: number;
        fallbackPlacements: string[];
        html: boolean;
        popperConfig: any;
        sanitize: boolean;
        sanitizeFn: any;
        selector: boolean;
        title: string;
    };
    static get DefaultType(): {
        content: string;
        allowList: string;
        animation: string;
        boundary: string;
        container: string;
        customClass: string;
        delay: string;
        fallbackPlacements: string;
        html: string;
        offset: string;
        placement: string;
        popperConfig: string;
        sanitize: string;
        sanitizeFn: string;
        selector: string;
        template: string;
        title: string;
        trigger: string;
    };
    _isWithContent(): any;
    _getContentForTemplate(): {
        ".popover-header": any;
        ".popover-body": any;
    };
    _getContent(): any;
}
declare const ScrollSpy_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class ScrollSpy extends ScrollSpy_base {
    static get Default(): {
        offset: any;
        rootMargin: string;
        smoothScroll: boolean;
        target: any;
        threshold: number[];
    };
    static get DefaultType(): {
        offset: string;
        rootMargin: string;
        smoothScroll: string;
        target: string;
        threshold: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    _targetLinks: Map<any, any>;
    _observableSections: Map<any, any>;
    _rootElement: any;
    _activeTarget: any;
    _observer: IntersectionObserver;
    _previousScrollData: {
        visibleEntryTop: number;
        parentScrollTop: number;
    };
    refresh(): void;
    _maybeEnableSmoothScroll(): void;
    _getNewObserver(): IntersectionObserver;
    _observerCallback(entries: any): void;
    _initializeTargetsAndObservables(): void;
    _process(target: any): void;
    _activateParents(target: any): void;
    _clearActiveClass(parent: any): void;
}
declare const Tab_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Tab extends Tab_base {
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    constructor(element: any);
    _parent: any;
    show(): void;
    _activate(element: any, relatedElem: any): void;
    _deactivate(element: any, relatedElem: any): void;
    _keydown(event: any): void;
    _getChildren(): any[];
    _getActiveElem(): any;
    _setInitialAttributes(parent: any, children: any): void;
    _setInitialAttributesOnChild(child: any): void;
    _setInitialAttributesOnTargetPanel(child: any): void;
    _toggleDropDown(element: any, open: any): void;
    _setAttributeIfNotExists(element: any, attribute: any, value: any): void;
    _elemIsActive(elem: any): any;
    _getInnerElement(elem: any): any;
    _getOuterElement(elem: any): any;
}
declare const Toast_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Toast extends Toast_base {
    static get Default(): {
        animation: boolean;
        autohide: boolean;
        delay: number;
    };
    static get DefaultType(): {
        animation: string;
        autohide: string;
        delay: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    _timeout: NodeJS.Timeout;
    _hasMouseInteraction: boolean;
    _hasKeyboardInteraction: boolean;
    show(): void;
    hide(): void;
    isShown(): any;
    _maybeScheduleHide(): void;
    _onInteraction(event: any, isInteracting: any): void;
    _setListeners(): void;
    _clearTimeout(): void;
}
declare const Tooltip_base: {
    new (element: any, config: any): {
        _element: any;
        _config: any;
        dispose(): void;
        _queueCallback(callback: any, element: any, isAnimated?: boolean): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
        _typeCheckConfig(config: any, configTypes?: any): void;
    };
    getInstance(element: any): any;
    getOrCreateInstance(element: any, config?: {}): any;
    readonly VERSION: string;
    readonly DATA_KEY: string;
    readonly EVENT_KEY: string;
    eventName(name: any): string;
    readonly Default: {};
    readonly DefaultType: {};
    readonly NAME: void;
};
/**
 * Class definition
 */
export class Tooltip extends Tooltip_base {
    static get Default(): {
        allowList: {
            '*': (string | RegExp)[];
            a: string[];
            area: any[];
            b: any[];
            br: any[];
            col: any[];
            code: any[];
            div: any[];
            em: any[];
            hr: any[];
            h1: any[];
            h2: any[];
            h3: any[];
            h4: any[];
            h5: any[];
            h6: any[];
            i: any[];
            img: string[];
            li: any[];
            ol: any[];
            p: any[];
            pre: any[];
            s: any[];
            small: any[];
            span: any[];
            sub: any[];
            sup: any[];
            strong: any[];
            u: any[];
            ul: any[];
        };
        animation: boolean;
        boundary: string;
        container: boolean;
        customClass: string;
        delay: number;
        fallbackPlacements: string[];
        html: boolean;
        offset: number[];
        placement: string;
        popperConfig: any;
        sanitize: boolean;
        sanitizeFn: any;
        selector: boolean;
        template: string;
        title: string;
        trigger: string;
    };
    static get DefaultType(): {
        allowList: string;
        animation: string;
        boundary: string;
        container: string;
        customClass: string;
        delay: string;
        fallbackPlacements: string;
        html: string;
        offset: string;
        placement: string;
        popperConfig: string;
        sanitize: string;
        sanitizeFn: string;
        selector: string;
        template: string;
        title: string;
        trigger: string;
    };
    static get NAME(): string;
    static jQueryInterface(config: any): any;
    _isEnabled: boolean;
    _timeout: number;
    _isHovered: boolean;
    _activeTrigger: {};
    _popper: any;
    _templateFactory: {
        _config: any;
        getContent(): any[];
        hasContent(): boolean;
        changeContent(content: any): any;
        toHtml(): Element;
        _typeCheckConfig(config: any): void;
        _checkContent(arg: any): void;
        _setContent(template: any, content: any, selector: any): void;
        _maybeSanitize(arg: any): any;
        _resolvePossibleFunction(arg: any): any;
        _putElementInTemplate(element: any, templateElement: any): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
    };
    _newContent: any;
    tip: Element;
    enable(): void;
    disable(): void;
    toggleEnabled(): void;
    toggle(): void;
    show(): void;
    hide(): void;
    update(): void;
    _isWithContent(): boolean;
    _getTipElement(): Element;
    _createTipElement(content: any): Element;
    setContent(content: any): void;
    _getTemplateFactory(content: any): {
        _config: any;
        getContent(): any[];
        hasContent(): boolean;
        changeContent(content: any): any;
        toHtml(): Element;
        _typeCheckConfig(config: any): void;
        _checkContent(arg: any): void;
        _setContent(template: any, content: any, selector: any): void;
        _maybeSanitize(arg: any): any;
        _resolvePossibleFunction(arg: any): any;
        _putElementInTemplate(element: any, templateElement: any): void;
        _getConfig(config: any): any;
        _configAfterMerge(config: any): any;
        _mergeConfigObj(config: any, element: any): any;
    };
    _getContentForTemplate(): {
        ".tooltip-inner": any;
    };
    _getTitle(): any;
    _initializeOnDelegatedTarget(event: any): any;
    _isAnimated(): any;
    _isShown(): boolean;
    _createPopper(tip: any): any;
    _getOffset(): any;
    _resolvePossibleFunction(arg: any): any;
    _getPopperConfig(attachment: any): any;
    _setListeners(): void;
    _hideModalHandler: () => void;
    _fixTitle(): void;
    _enter(): void;
    _leave(): void;
    _setTimeout(handler: any, timeout: any): void;
    _isWithActiveTrigger(): boolean;
    _getDelegateConfig(): {
        selector: boolean;
        trigger: string;
    };
    _disposePopper(): void;
}
export {};
