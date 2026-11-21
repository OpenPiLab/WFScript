#pragma once

#include <QString>

// One worker-thread QJSEngine owns subscriptions and computed definitions.
// Values and bindings remain owned by the native typed PanelUiBridge.
// No QJSValue or QObject crosses the GUI/PanelRuntime thread boundary.
namespace PanelDataEvents {
inline QString bootstrapSource() {
    return QStringLiteral(R"JS(
        var __dataListeners = Object.create(null);
        var __dataNextListenerId = 1;
        var __dataEventQueue = [];
        var __dataDispatching = false;
        var __dataSubscriptionCount = 0;
        var __dataMaxSubscriptions = 256;
        var __dataMaxEventsPerCycle = 256;
        var __dataComputed = Object.create(null);
        var __dataComputedCount = 0;
        var __dataMaxComputed = 64;
        var __dataEvaluating = false;

        function __dataReportCallbackError(name,error) {
            __panelHost.reportDataCallbackError(name,error);
        }
        function __dataEvaluate(def, staged) {
            var args = def.deps.map(function(dep) {
                return Object.prototype.hasOwnProperty.call(staged,dep) ? staged[dep] : Data.get(dep);
            });
            // Computed functions are pure transformations. Re-entrant writes
            // would make topological evaluation order non-deterministic.
            if(__dataEvaluating) throw new Error('Nested Data.computed evaluation is not supported');
            __dataEvaluating = true;
            var value;
            try { value = def.fn.apply(undefined,args); }
            finally { __dataEvaluating = false; }
            if(!__panelHost.validateComputedValue(def.name,value))
                throw new TypeError('Data.computed('+def.name+'): '+__panelHost.lastDataError());
            return value;
        }
        function __dataRecomputeAfter(source) {
            // Find descendants in the declared dependency graph. A formula is
            // evaluated at most once per source event, even in a diamond.
            var affected = Object.create(null);
            affected[source] = true;
            var names = Object.keys(__dataComputed);
            var more = true;
            while(more) {
                more = false;
                for(var i=0;i<names.length;++i) {
                    var name=names[i], def=__dataComputed[name];
                    if(affected[name]) continue;
                    if(def.deps.some(function(dep){return !!affected[dep];})) {
                        affected[name]=true;
                        more=true;
                    }
                }
            }
            var order=[],visited=Object.create(null);
            function visit(name) {
                if(visited[name] || !affected[name] || name===source) return;
                visited[name]=true;
                var deps=__dataComputed[name].deps;
                for(var i=0;i<deps.length;++i)
                    if(__dataComputed[deps[i]] && affected[deps[i]]) visit(deps[i]);
                order.push(name);
            }
            for(var j=0;j<names.length;++j)visit(names[j]);
            var staged=Object.create(null), changed=Object.create(null), updates=[];
            changed[source]=true;
            for(var k=0;k<order.length;++k) {
                var target=order[k], formula=__dataComputed[target];
                if(!formula.deps.some(function(dep){return !!changed[dep];})) continue;
                var result=__dataEvaluate(formula,staged);
                var previous=Data.get(target);
                if(result!==previous) {
                    staged[target]=result;
                    changed[target]=true;
                    updates.push({name:target,value:result,previous:previous});
                }
            }
            // All formulas and native types have been validated BEFORE any
            // derived value is committed. Commit in dependency order.
            for(var n=0;n<updates.length;++n) {
                var update=updates[n];
                if(!__panelHost.setComputedData(update.name,update.value))
                    throw new Error('Data.computed('+update.name+'): '+__panelHost.lastDataError());
            }
            return updates;
        }
        function __panelDataDispatch(name,value,previousValue) {
            // Synchronous propagation makes Data.get(derived) up-to-date by
            // the time any source onChange callback executes. A failed formula
            // reports a diagnostic and leaves prior derived values unchanged;
            // the source event is still delivered to its listeners.
            var derived=[];
            try { derived=__dataRecomputeAfter(name); }
            catch(error) { __dataReportCallbackError('Data.computed after '+name,error); }
            __dataEventQueue.push({name:name,value:value,previous:previousValue});
            for(var d=0;d<derived.length;++d)__dataEventQueue.push(derived[d]);
            if(__dataDispatching) return true;
            __dataDispatching = true;
            var processed = 0;
            try {
                while(__dataEventQueue.length) {
                    if(++processed > __dataMaxEventsPerCycle) {
                        __dataEventQueue.length = 0;
                        throw new Error('Panel Data change cycle exceeded 256 events; check Data.onChange for a feedback loop');
                    }
                    var event = __dataEventQueue.shift();
                    var ids = Object.keys(__dataListeners);
                    for(var i=0; i<ids.length; ++i) {
                        var listener = __dataListeners[ids[i]];
                        if(!listener || listener.name !== event.name) continue;
                        try { listener.callback(event.value,event.previous,event.name); }
                        catch(error) { __dataReportCallbackError(event.name,error); }
                    }
                }
            } finally {
                __dataDispatching = false;
            }
            return true;
        }
        var Data = {
            get: function(name) {
                if (!__panelHost.hasData(name)) throw new Error('Unknown Panel Data variable: '+name);
                return __panelHost.getData(name);
            },
            set: function(name,value) {
                if(__dataEvaluating) throw new Error('Data.set is not allowed inside Data.computed formulas');
                if(Object.prototype.hasOwnProperty.call(__dataComputed,name))
                    throw new Error('Panel Data '+name+' is computed and read-only');
                var before = Data.get(name);
                if (!__panelHost.setData(name,value)) throw new Error(__panelHost.lastDataError());
                var after = __panelHost.getData(name);
                if(before !== after) __panelDataDispatch(name,after,before);
                return true;
            },
            names: function() { return __panelHost.dataNamesJs(); },
            computed: function(name,deps,callback) {
                if(__dataEvaluating) throw new Error('Cannot register Data.computed from a computed formula');
                if(typeof name!=='string' || !__panelHost.hasData(name))
                    throw new Error('Data.computed target must be a declared Panel Data variable: '+name);
                if(Object.prototype.hasOwnProperty.call(__dataComputed,name))
                    throw new Error('Data.computed already registered: '+name);
                if(__dataComputedCount>=__dataMaxComputed)
                    throw new Error('Data.computed limit: 64 formulas');
                if(!Array.isArray(deps) || !deps.length || deps.length>16)
                    throw new TypeError('Data.computed dependencies must be an array of 1..16 variable names');
                if(typeof callback!=='function') throw new TypeError('Data.computed requires a function');
                var unique=Object.create(null),copy=[];
                for(var i=0;i<deps.length;++i) {
                    var dep=deps[i];
                    if(typeof dep!=='string' || !__panelHost.hasData(dep))
                        throw new Error('Data.computed('+name+') has an unknown dependency: '+dep);
                    if(unique[dep]) throw new Error('Data.computed('+name+') duplicate dependency: '+dep);
                    unique[dep]=true;
                    copy.push(dep);
                }
                // A new edge name -> dependency forms a cycle if following
                // existing dependency edges can reach name.
                var pending=copy.slice(),seen=Object.create(null);
                while(pending.length) {
                    var current=pending.pop();
                    if(current===name)throw new Error('Data.computed circular dependency involving '+name);
                    if(seen[current])continue;
                    seen[current]=true;
                    if(__dataComputed[current])
                        pending=pending.concat(__dataComputed[current].deps);
                }
                var def={name:name,deps:copy,fn:callback};
                // Evaluate and type-check BEFORE locking the native target.
                var initial=__dataEvaluate(def,Object.create(null));
                if(!__panelHost.registerComputedData(name))
                    throw new Error('Data.computed('+name+'): '+__panelHost.lastDataError());
                __dataComputed[name]=def;
                ++__dataComputedCount;
                var previous=Data.get(name);
                if(initial!==previous) {
                    if(!__panelHost.setComputedData(name,initial))
                        throw new Error('Data.computed('+name+'): '+__panelHost.lastDataError());
                    __panelDataDispatch(name,initial,previous);
                }
                return name;
            },
            isComputed: function(name) {
                if(typeof name!=='string' || !__panelHost.hasData(name))
                    throw new Error('Unknown Panel Data variable: '+name);
                return Object.prototype.hasOwnProperty.call(__dataComputed,name);
            },
            onChange: function(name,callback,options) {
                if(typeof name !== 'string' || !__panelHost.hasData(name))
                    throw new Error('Unknown Panel Data variable: '+name);
                if(typeof callback !== 'function') throw new TypeError('Data.onChange requires a callback function');
                if(options !== undefined) {
                    if(options === null || typeof options !== 'object' || Array.isArray(options))
                        throw new TypeError('Data.onChange options must be an object');
                    if(Object.keys(options).some(function(k){return k !== 'immediate';}) ||
                       (options.immediate !== undefined && typeof options.immediate !== 'boolean'))
                        throw new TypeError('Data.onChange supports only {immediate: boolean}');
                }
                if(__dataSubscriptionCount >= __dataMaxSubscriptions)
                    throw new Error('Data.onChange limit: 256 active subscriptions');
                var id = __dataNextListenerId++;
                __dataListeners[id] = {name:name,callback:callback};
                ++__dataSubscriptionCount;
                if(options && options.immediate) {
                    try { callback(Data.get(name),undefined,name); }
                    catch(error) { __dataReportCallbackError(name,error); }
                }
                return id;
            },
            offChange: function(id) {
                if(typeof id !== 'number' || !Number.isSafeInteger(id) || id < 1 ||
                   !Object.prototype.hasOwnProperty.call(__dataListeners,id)) return false;
                delete __dataListeners[id];
                --__dataSubscriptionCount;
                return true;
            }
        };
    )JS");
}
}
