// Copyright 2006-2008 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// Touch the RegExp and Date functions to make sure that date-delay.js and
// regexp-delay.js has been loaded. This is required as the mirrors use
// functions within these files through the builtins object. See the
// function DateToISO8601_ as an example.
RegExp;
Date;


/**
 * Returns the mirror for a specified value or object.
 *
 * @param {value or Object} value the value or object to retreive the mirror for
 * @returns {Mirror} the mirror reflects the passed value or object
 */
function MakeMirror(value) {
  if (IS_UNDEFINED(value)) return new UndefinedMirror();
  if (IS_NULL(value)) return new NullMirror();
  if (IS_BOOLEAN(value)) return new BooleanMirror(value);
  if (IS_NUMBER(value)) return new NumberMirror(value);
  if (IS_STRING(value)) return new StringMirror(value);
  if (IS_ARRAY(value)) return new ArrayMirror(value);
  if (IS_DATE(value)) return new DateMirror(value);
  if (IS_FUNCTION(value)) return new FunctionMirror(value);
  if (IS_REGEXP(value)) return new RegExpMirror(value);
  if (IS_ERROR(value)) return new ErrorMirror(value);
  return new ObjectMirror(value);
}


/**
 * Inherit the prototype methods from one constructor into another.
 *
 * The Function.prototype.inherits from lang.js rewritten as a standalone
 * function (not on Function.prototype). NOTE: If this file is to be loaded
 * during bootstrapping this function needs to be revritten using some native
 * functions as prototype setup using normal JavaScript does not work as
 * expected during bootstrapping (see mirror.js in r114903).
 *
 * @param {function} ctor Constructor function which needs to inherit the
 *     prototype
 * @param {function} superCtor Constructor function to inherit prototype from
 */
function inherits(ctor, superCtor) {
  var tempCtor = function(){};
  tempCtor.prototype = superCtor.prototype;
  ctor.super_ = superCtor.prototype;
  ctor.prototype = new tempCtor();
  ctor.prototype.constructor = ctor;
}


// Type names of the different mirrors.
const UNDEFINED_TYPE = 'undefined';
const NULL_TYPE = 'null';
const BOOLEAN_TYPE = 'boolean';
const NUMBER_TYPE = 'number';
const STRING_TYPE = 'string';
const OBJECT_TYPE = 'object';
const FUNCTION_TYPE = 'function';
const REGEXP_TYPE = 'regexp';
const ERROR_TYPE = 'error';
const PROPERTY_TYPE = 'property';
const ACCESSOR_TYPE = 'accessor';
const FRAME_TYPE = 'frame';
const SCRIPT_TYPE = 'script';

// Maximum length when sending strings through the JSON protocol.
const kMaxProtocolStringLength = 80;

// Different kind of properties.
PropertyKind = {};
PropertyKind.Named   = 1;
PropertyKind.Indexed = 2;


// A copy of the PropertyType enum from global.h
PropertyType = {};
PropertyType.Normal             = 0;
PropertyType.Field              = 1;
PropertyType.ConstantFunction   = 2;
PropertyType.Callbacks          = 3;
PropertyType.Interceptor        = 4;
PropertyType.MapTransition      = 5;
PropertyType.ConstantTransition = 6;
PropertyType.NullDescriptor     = 7;


// Different attributes for a property.
PropertyAttribute = {};
PropertyAttribute.None       = NONE;
PropertyAttribute.ReadOnly   = READ_ONLY;
PropertyAttribute.DontEnum   = DONT_ENUM;
PropertyAttribute.DontDelete = DONT_DELETE;


// Mirror hierarchy:
//   - Mirror
//     - ValueMirror
//       - UndefinedMirror
//       - NullMirror
//       - NumberMirror
//       - StringMirror
//       - ObjectMirror
//         - FunctionMirror
//           - UnresolvedFunctionMirror
//         - ArrayMirror
//         - DateMirror
//         - RegExpMirror
//         - ErrorMirror
//     - PropertyMirror
//       - InterceptorPropertyMirror
//     - AccessorMirror
//     - FrameMirror
//     - ScriptMirror


/**
 * Base class for all mirror objects.
 * @param {string} type The type of the mirror
 * @constructor
 */
function Mirror(type) {
  this.type_ = type;
};


Mirror.prototype.type = function() {
  return this.type_;
};


/**
 * Check whether the mirror reflects the undefined value.
 * @returns {boolean} True if the mirror reflects the undefined value.
 */
Mirror.prototype.isUndefined = function() {
  return this instanceof UndefinedMirror;
}


/**
 * Check whether the mirror reflects the null value.
 * @returns {boolean} True if the mirror reflects the null value
 */
Mirror.prototype.isNull = function() {
  return this instanceof NullMirror;
}


/**
 * Check whether the mirror reflects a boolean value.
 * @returns {boolean} True if the mirror reflects a boolean value
 */
Mirror.prototype.isBoolean = function() {
  return this instanceof BooleanMirror;
}


/**
 * Check whether the mirror reflects a number value.
 * @returns {boolean} True if the mirror reflects a number value
 */
Mirror.prototype.isNumber = function() {
  return this instanceof NumberMirror;
}


/**
 * Check whether the mirror reflects a string value.
 * @returns {boolean} True if the mirror reflects a string value
 */
Mirror.prototype.isString = function() {
  return this instanceof StringMirror;
}


/**
 * Check whether the mirror reflects an object.
 * @returns {boolean} True if the mirror reflects an object
 */
Mirror.prototype.isObject = function() {
  return this instanceof ObjectMirror;
}


/**
 * Check whether the mirror reflects a function.
 * @returns {boolean} True if the mirror reflects a function
 */
Mirror.prototype.isFunction = function() {
  return this instanceof FunctionMirror;
}


/**
 * Check whether the mirror reflects an unresolved function.
 * @returns {boolean} True if the mirror reflects an unresolved function
 */
Mirror.prototype.isUnresolvedFunction = function() {
  return this instanceof UnresolvedFunctionMirror;
}


/**
 * Check whether the mirror reflects an array.
 * @returns {boolean} True if the mirror reflects an array
 */
Mirror.prototype.isArray = function() {
  return this instanceof ArrayMirror;
}


/**
 * Check whether the mirror reflects a date.
 * @returns {boolean} True if the mirror reflects a date
 */
Mirror.prototype.isDate = function() {
  return this instanceof DateMirror;
}


/**
 * Check whether the mirror reflects a regular expression.
 * @returns {boolean} True if the mirror reflects a regular expression
 */
Mirror.prototype.isRegExp = function() {
  return this instanceof RegExpMirror;
}


/**
 * Check whether the mirror reflects an error.
 * @returns {boolean} True if the mirror reflects an error
 */
Mirror.prototype.isError = function() {
  return this instanceof ErrorMirror;
}


/**
 * Check whether the mirror reflects a property.
 * @returns {boolean} True if the mirror reflects a property
 */
Mirror.prototype.isProperty = function() {
  return this instanceof PropertyMirror;
}


/**
 * Check whether the mirror reflects a property from an interceptor.
 * @returns {boolean} True if the mirror reflects a property from an
 *     interceptor
 */
Mirror.prototype.isInterceptorProperty = function() {
  return this instanceof InterceptorPropertyMirror;
}


/**
 * Check whether the mirror reflects an accessor.
 * @returns {boolean} True if the mirror reflects an accessor
 */
Mirror.prototype.isAccessor = function() {
  return this instanceof AccessorMirror;
}


/**
 * Check whether the mirror reflects a stack frame.
 * @returns {boolean} True if the mirror reflects a stack frame
 */
Mirror.prototype.isFrame = function() {
  return this instanceof FrameMirror;
}


/**
 * Check whether the mirror reflects a script.
 * @returns {boolean} True if the mirror reflects a script
 */
Mirror.prototype.isScript = function() {
  return this instanceof ScriptMirror;
}


/**
 * Serialize object in JSON format. The actual serialization is handled by the
 * JSONProtocolSerializer.
 * @param {boolean} details Indicate level of details to include
 * @return {string} JSON serialization
 */
Mirror.prototype.toJSONProtocol = function(details) {
  var serializer = new JSONProtocolSerializer(details)
  return serializer.serialize(this)
}


Mirror.prototype.toText = function() {
  // Simpel to text which is used when on specialization in subclass.
  return "#<" + builtins.GetInstanceName(this.constructor.name) + ">";
}


/**
 * Base class for all value mirror objects.
 * @param {string} type The type of the mirror
 * @param {value} value The value reflected by this mirror
 * @constructor
 * @extends Mirror
 */
function ValueMirror(type, value) {
  Mirror.call(this, type);
  this.value_ = value;
}
inherits(ValueMirror, Mirror);


/**
 * Check whether this is a primitive value.
 * @return {boolean} True if the mirror reflects a primitive value
 */
ValueMirror.prototype.isPrimitive = function() {
  var type = this.type();
  return type === 'undefined' ||
         type === 'null' ||
         type === 'boolean' ||
         type === 'number' ||
         type === 'string';
};


 /**
 * Get the actual value reflected by this mirror.
 * @return {value} The value reflected by this mirror
 */
ValueMirror.prototype.value = function() {
  return this.value_;
};


/**
 * Mirror object for Undefined.
 * @constructor
 * @extends ValueMirror
 */
function UndefinedMirror() {
  ValueMirror.call(this, UNDEFINED_TYPE, void 0);
}
inherits(UndefinedMirror, ValueMirror);


UndefinedMirror.prototype.toText = function() {
  return 'undefined';
}


/**
 * Mirror object for null.
 * @constructor
 * @extends ValueMirror
 */
function NullMirror() {
  ValueMirror.call(this, NULL_TYPE, null);
}
inherits(NullMirror, ValueMirror);


NullMirror.prototype.toText = function() {
  return 'null';
}


/**
 * Mirror object for boolean values.
 * @param {boolean} value The boolean value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function BooleanMirror(value) {
  ValueMirror.call(this, BOOLEAN_TYPE, value);
}
inherits(BooleanMirror, ValueMirror);


BooleanMirror.prototype.toText = function() {
  return this.value_ ? 'true' : 'false';
}


/**
 * Mirror object for number values.
 * @param {number} value The number value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function NumberMirror(value) {
  ValueMirror.call(this, NUMBER_TYPE, value);
}
inherits(NumberMirror, ValueMirror);


NumberMirror.prototype.toText = function() {
  return %NumberToString(this.value_);
}


/**
 * Mirror object for string values.
 * @param {string} value The string value reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function StringMirror(value) {
  ValueMirror.call(this, STRING_TYPE, value);
}
inherits(StringMirror, ValueMirror);


StringMirror.prototype.length = function() {
  return this.value_.length;
};


StringMirror.prototype.toText = function() {
  if (this.length() > kMaxProtocolStringLength) {
    return this.value_.substring(0, kMaxProtocolStringLength) +
           '... (length: ' + this.length() + ')';
  } else {
    return this.value_;
  }
}


/**
 * Mirror object for objects.
 * @param {object} value The object reflected by this mirror
 * @constructor
 * @extends ValueMirror
 */
function ObjectMirror(value, type) {
  ValueMirror.call(this, type || OBJECT_TYPE, value);
}
inherits(ObjectMirror, ValueMirror);


ObjectMirror.prototype.className = function() {
  return %ClassOf(this.value_);
};


ObjectMirror.prototype.constructorFunction = function() {
  return MakeMirror(%DebugGetProperty(this.value_, 'constructor'));
};


ObjectMirror.prototype.prototypeObject = function() {
  return MakeMirror(%DebugGetProperty(this.value_, 'prototype'));
};


ObjectMirror.prototype.protoObject = function() {
  return MakeMirror(%GetPrototype(this.value_));
};


ObjectMirror.prototype.hasNamedInterceptor = function() {
  // Get information on interceptors for this object.
  var x = %DebugInterceptorInfo(this.value_);
  return (x & 2) != 0;
};


ObjectMirror.prototype.hasIndexedInterceptor = function() {
  // Get information on interceptors for this object.
  var x = %DebugInterceptorInfo(this.value_);
  return (x & 1) != 0;
};


/**
 * Return the property names for this object.
 * @param {number} kind Indicate whether named, indexed or both kinds of
 *     properties are requested
 * @param {number} limit Limit the number of names returend to the specified
       value
 * @return {Array} Property names for this object
 */
ObjectMirror.prototype.propertyNames = function(kind, limit) {
  // Find kind and limit and allocate array for the result
  kind = kind || PropertyKind.Named | PropertyKind.Indexed;

  var propertyNames;
  var elementNames;
  var total = 0;
  if (kind & PropertyKind.Named) {
    propertyNames = %DebugLocalPropertyNames(this.value_);
    total += propertyNames.length;
  }
  if (kind & PropertyKind.Indexed) {
    elementNames = %DebugLocalElementNames(this.value_)
    total += elementNames.length;
  }
  limit = Math.min(limit || total, total);

  var names = new Array(limit);
  var index = 0;

  // Copy names for named properties.
  if (kind & PropertyKind.Named) {
    for (var i = 0; index < limit && i < propertyNames.length; i++) {
      names[index++] = propertyNames[i];
    }
  }

  // Copy names for indexed properties.
  if (kind & PropertyKind.Indexed) {
    for (var i = 0; index < limit && i < elementNames.length; i++) {
      names[index++] = elementNames[i];
    }
  }

  return names;
};


/**
 * Return the properties for this object as an array of PropertyMirror objects.
 * @param {number} kind Indicate whether named, indexed or both kinds of
 *     properties are requested
 * @param {number} limit Limit the number of properties returend to the
       specified value
 * @return {Array} Property mirrors for this object
 */
ObjectMirror.prototype.properties = function(kind, limit) {
  var names = this.propertyNames(kind, limit);
  var properties = new Array(names.length);
  for (var i = 0; i < names.length; i++) {
    properties[i] = this.property(names[i]);
  }

  return properties;
};


/**
 * Return the interceptor property names for this object.
 * @param {number} kind Indicate whether named, indexed or both kinds of
 *     interceptor properties are requested
 * @param {number} limit Limit the number of names returend to the specified
       value
 * @return {Array} interceptor property names for this object
 */
ObjectMirror.prototype.interceptorPropertyNames = function(kind, limit) {
  // Find kind.
  kind = kind || PropertyKind.Named | PropertyKind.Indexed;
  var namedInterceptorNames;
  var indexedInterceptorNames;

  // Get names for named interceptor properties.
  if (this.hasNamedInterceptor() && kind & PropertyKind.Named) {
    namedInterceptorNames = %DebugNamedInterceptorPropertyNames(this.value_);
  }

  // Get names for indexed interceptor properties.
  if (this.hasIndexedInterceptor() && kind & PropertyKind.Indexed) {
    indexedInterceptorNames = %DebugIndexedInterceptorElementNames(this.value_);
  }

  // Return either retult or both concattenated.
  if (namedInterceptorNames && indexedInterceptorNames) {
    return namedInterceptorNames.concat(indexedInterceptorNames);
  } else if (namedInterceptorNames) {
    return namedInterceptorNames;
  } else if (indexedInterceptorNames) {
    return indexedInterceptorNames;
  } else {
    return new Array(0);
  }
};


/**
 * Return interceptor properties this object.
 * @param {number} opt_kind Indicate whether named, indexed or both kinds of
 *     interceptor properties are requested
 * @param {Array} opt_names Limit the number of properties returned to the
       specified value
 * @return {Array} properties this object as an array of PropertyMirror objects
 */
ObjectMirror.prototype.interceptorProperties = function(opt_kind, opt_names) {
  // Find kind.
  var kind = opt_kind || PropertyKind.Named | PropertyKind.Indexed;
  var namedInterceptorProperties;
  var indexedInterceptorProperties;

  // Get values for named interceptor properties.
  if (kind & PropertyKind.Named) {
    var names = opt_names || this.interceptorPropertyNames(PropertyKind.Named);
    namedInterceptorProperties = new Array(names.length);
    for (i = 0; i < names.length; i++) {
      var value = %DebugNamedInterceptorPropertyValue(this.value_, names[i]);
      namedInterceptorProperties[i] = new InterceptorPropertyMirror(this, names[i], value);
    }
  }

  // Get values for indexed interceptor properties.
  if (kind & PropertyKind.Indexed) {
    var names = opt_names || this.interceptorPropertyNames(PropertyKind.Indexed);
    indexedInterceptorProperties = new Array(names.length);
    for (i = 0; i < names.length; i++) {
      // Don't try to get the value if the name is not a number.
      if (IS_NUMBER(names[i])) {
        var value = %DebugIndexedInterceptorElementValue(this.value_, names[i]);
        indexedInterceptorProperties[i] = new InterceptorPropertyMirror(this, names[i], value);
      }
    }
  }

  // Return either result or both concattenated.
  if (namedInterceptorProperties && indexedInterceptorProperties) {
    return namedInterceptorProperties.concat(indexedInterceptorProperties);
  } else if (namedInterceptorProperties) {
    return namedInterceptorProperties;
  } else {
    return indexedInterceptorProperties;
  }
};


ObjectMirror.prototype.property = function(name) {
  var details = %DebugGetPropertyDetails(this.value_, %ToString(name));
  if (details) {
    return new PropertyMirror(this, name, details[0], details[1]);
  }

  // Nothing found.
  return new UndefinedMirror();
};



/**
 * Try to find a property from its value.
 * @param {Mirror} value The property value to look for
 * @return {PropertyMirror} The property with the specified value. If no
 *     property was found with the specified value UndefinedMirror is returned
 */
ObjectMirror.prototype.lookupProperty = function(value) {
  var properties = this.properties();

  // Look for property value in properties.
  for (var i = 0; i < properties.length; i++) {

    // Skip properties which are defined through assessors.
    var property = properties[i];
    if (property.propertyType() != PropertyType.Callbacks) {
      if (%_ObjectEquals(property.value_, value.value_)) {
        return property;
      }
    }
  }

  // Nothing found.
  return new UndefinedMirror();
};


/**
 * Returns objects which has direct references to this object
 * @param {number} opt_max_instances Optional parameter specifying the maximum
 *     number of instances to return.
 * @return {Array} The objects which has direct references to this object.
 */
ObjectMirror.prototype.referencedBy = function(opt_max_instances) {
  // Find all objects constructed from this function.
  var result = %DebugReferencedBy(this.value_, Mirror.prototype, opt_max_instances || 0);

  // Make mirrors for all the instances found.
  for (var i = 0; i < result.length; i++) {
    result[i] = MakeMirror(result[i]);
  }

  return result;
};


ObjectMirror.prototype.toText = function() {
  var name;
  var ctor = this.constructorFunction();
  if (ctor.isUndefined()) {
    name = this.className();
  } else {
    name = ctor.name();
    if (!name) {
      name = this.className();
    }
  }
  return '#<' + builtins.GetInstanceName(name) + '>';
};


/**
 * Mirror object for functions.
 * @param {function} value The function object reflected by this mirror.
 * @constructor
 * @extends ObjectMirror
 */
function FunctionMirror(value) {
  ObjectMirror.call(this, value, FUNCTION_TYPE);
  this.resolved_ = true;
}
inherits(FunctionMirror, ObjectMirror);


/**
 * Returns whether the function is resolved.
 * @return {boolean} True if the function is resolved. Unresolved functions can
 *     only originate as functions from stack frames
 */
FunctionMirror.prototype.resolved = function() {
  return this.resolved_;
};


/**
 * Returns the name of the function.
 * @return {string} Name of the function
 */
FunctionMirror.prototype.name = function() {
  return %FunctionGetName(this.value_);
};


/**
 * Returns the source code for the function.
 * @return {string or undefined} The source code for the function. If the
 *     function is not resolved undefined will be returned.
 */
FunctionMirror.prototype.source = function() {
  // Return source if function is resolved. Otherwise just fall through to
  // return undefined.
  if (this.resolved()) {
    // This builtins function is context independant (only uses runtime
    // calls and typeof.
    return builtins.FunctionSourceString(this.value_);
  }
};


/**
 * Returns the script object for the function.
 * @return {ScriptMirror or undefined} Script object for the function or
 *     undefined if the function has no script
 */
FunctionMirror.prototype.script = function() {
  // Return script if function is resolved. Otherwise just fall through
  // to return undefined.
  if (this.resolved()) {
    var script = %FunctionGetScript(this.value_);
    if (script) {
      return new ScriptMirror(script);
    }
  }
};


/**
 * Returns objects constructed by this function.
 * @param {number} opt_max_instances Optional parameter specifying the maximum
 *     number of instances to return.
 * @return {Array or undefined} The objects constructed by this function.
 */
FunctionMirror.prototype.constructedBy = function(opt_max_instances) {
  if (this.resolved()) {
    // Find all objects constructed from this function.
    var result = %DebugConstructedBy(this.value_, opt_max_instances || 0);

    // Make mirrors for all the instances found.
    for (var i = 0; i < result.length; i++) {
      result[i] = MakeMirror(result[i]);
    }

    return result;
  } else {
    return [];
  }
};


FunctionMirror.prototype.toText = function() {
  return this.source();
}


/**
 * Mirror object for unresolved functions.
 * @param {string} value The name for the unresolved function reflected by this
 *     mirror.
 * @constructor
 * @extends ObjectMirror
 */
function UnresolvedFunctionMirror(value) {
  // Construct this using the ValueMirror as an unresolved function is not a
  // real object but just a string.
  ValueMirror.call(this, FUNCTION_TYPE, value);
  this.propertyCount_ = 0;
  this.elementCount_ = 0;
  this.resolved_ = false;
}
inherits(UnresolvedFunctionMirror, FunctionMirror);


UnresolvedFunctionMirror.prototype.className = function() {
  return 'Function';
};


UnresolvedFunctionMirror.prototype.constructorFunction = function() {
  return new UndefinedMirror();
};


UnresolvedFunctionMirror.prototype.prototypeObject = function() {
  return new UndefinedMirror();
};


UnresolvedFunctionMirror.prototype.protoObject = function() {
  return new UndefinedMirror();
};


UnresolvedFunctionMirror.prototype.name = function() {
  return this.value_;
};


UnresolvedFunctionMirror.prototype.propertyNames = function(kind, limit) {
  return [];
}


/**
 * Mirror object for arrays.
 * @param {Array} value The Array object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function ArrayMirror(value) {
  ObjectMirror.call(this, value);
}
inherits(ArrayMirror, ObjectMirror);


ArrayMirror.prototype.length = function() {
  return this.value_.length;
};


ArrayMirror.prototype.indexedPropertiesFromRange = function(opt_from_index, opt_to_index) {
  var from_index = opt_from_index || 0;
  var to_index = opt_to_index || this.length() - 1;
  if (from_index > to_index) return new Array();
  var values = new Array(to_index - from_index + 1);
  for (var i = from_index; i <= to_index; i++) {
    var details = %DebugGetPropertyDetails(this.value_, %ToString(i));
    var value;
    if (details) {
      value = new PropertyMirror(this, i, details[0], details[1]);
    } else {
      value = new UndefinedMirror();
    }
    values[i - from_index] = value;
  }
  return values;
}


/**
 * Mirror object for dates.
 * @param {Date} value The Date object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function DateMirror(value) {
  ObjectMirror.call(this, value);
}
inherits(DateMirror, ObjectMirror);


DateMirror.prototype.toText = function() {
  return DateToISO8601_(this.value_);
}


/**
 * Mirror object for regular expressions.
 * @param {RegExp} value The RegExp object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function RegExpMirror(value) {
  ObjectMirror.call(this, value, REGEXP_TYPE);
}
inherits(RegExpMirror, ObjectMirror);


/**
 * Returns the source to the regular expression.
 * @return {string or undefined} The source to the regular expression
 */
RegExpMirror.prototype.source = function() {
  return this.value_.source;
};


/**
 * Returns whether this regular expression has the global (g) flag set.
 * @return {boolean} Value of the global flag
 */
RegExpMirror.prototype.global = function() {
  return this.value_.global;
};


/**
 * Returns whether this regular expression has the ignore case (i) flag set.
 * @return {boolean} Value of the ignore case flag
 */
RegExpMirror.prototype.ignoreCase = function() {
  return this.value_.ignoreCase;
};


/**
 * Returns whether this regular expression has the multiline (m) flag set.
 * @return {boolean} Value of the multiline flag
 */
RegExpMirror.prototype.multiline = function() {
  return this.value_.multiline;
};


RegExpMirror.prototype.toText = function() {
  // Simpel to text which is used when on specialization in subclass.
  return "/" + this.source() + "/";
}


/**
 * Mirror object for error objects.
 * @param {Error} value The error object reflected by this mirror
 * @constructor
 * @extends ObjectMirror
 */
function ErrorMirror(value) {
  ObjectMirror.call(this, value, ERROR_TYPE);
}
inherits(ErrorMirror, ObjectMirror);


/**
 * Returns the message for this eror object.
 * @return {string or undefined} The message for this eror object
 */
ErrorMirror.prototype.message = function() {
  return this.value_.message;
};


ErrorMirror.prototype.toText = function() {
  // Use the same text representation as in messages.js.
  var text;
  try {
    str = builtins.ToDetailString(this.value_);
  } catch (e) {
    str = '#<an Error>';
  }
  return str;
}


/**
 * Base mirror object for properties.
 * @param {ObjectMirror} mirror The mirror object having this property
 * @param {string} name The name of the property
 * @param {Object} value The value of the property
 * @constructor
 * @extends Mirror
 */
function PropertyMirror(mirror, name, value, details) {
  Mirror.call(this, PROPERTY_TYPE);
  this.mirror_ = mirror;
  this.name_ = name;
  this.value_ = value;
  this.details_ = details;
}
inherits(PropertyMirror, Mirror);


PropertyMirror.prototype.isReadOnly = function() {
  return (this.attributes() & PropertyAttribute.ReadOnly) != 0;
}


PropertyMirror.prototype.isEnum = function() {
  return (this.attributes() & PropertyAttribute.DontEnum) == 0;
}


PropertyMirror.prototype.canDelete = function() {
  return (this.attributes() & PropertyAttribute.DontDelete) == 0;
}


PropertyMirror.prototype.name = function() {
  return this.name_;
}


PropertyMirror.prototype.isIndexed = function() {
  for (var i = 0; i < this.name_.length; i++) {
    if (this.name_[i] < '0' || '9' < this.name_[i]) {
      return false;
    }
  }
  return true;
}


PropertyMirror.prototype.value = function() {
  if (this.propertyType() == PropertyType.Callbacks) {
    // TODO(1242933): AccessorMirror should have getter/setter values.
    return new AccessorMirror();
  } else if (this.type() == PropertyType.Interceptor) {
    return new UndefinedMirror();
  } else {
    return MakeMirror(this.value_);
  }
}


PropertyMirror.prototype.attributes = function() {
  return %DebugPropertyAttributesFromDetails(this.details_);
}


PropertyMirror.prototype.propertyType = function() {
  return %DebugPropertyTypeFromDetails(this.details_);
}


PropertyMirror.prototype.insertionIndex = function() {
  return %DebugPropertyIndexFromDetails(this.details_);
}


/**
 * Mirror object for interceptor named properties.
 * @param {ObjectMirror} mirror The mirror object having this property
 * @param {String} name The name of the property
 * @param {value} value The value of the property
 * @constructor
 * @extends PropertyMirror
 */
function InterceptorPropertyMirror(mirror, name, value) {
  PropertyMirror.call(this, mirror, name, value, PropertyType.Interceptor);
}
inherits(InterceptorPropertyMirror, PropertyMirror);


/**
 * Mirror object for property accessors.
 * @param {Function} getter The getter function for this accessor
 * @param {Function} setter The setter function for this accessor
 * @constructor
 * @extends Mirror
 */
function AccessorMirror(getter, setter) {
  Mirror.call(this, ACCESSOR_TYPE);
  this.getter_ = getter;
  this.setter_ = setter;
}
inherits(AccessorMirror, Mirror);


/**
 * Returns whether this accessor is native or not. A native accessor is either
 * a VM buildin or provided through the API. A non native accessor is defined
 * in JavaScript using the __defineGetter__ and/or __defineGetter__ functions.
 * @return {boolean} True is the accessor is native
 */
AccessorMirror.prototype.isNative = function() {
  return IS_UNDEFINED(this.getter_) && IS_UNDEFINED(this.setter_);
}


/**
 * Returns a mirror for the function of a non native getter.
 * @return {FunctionMirror} Function mirror for the getter set using
 *     __defineGetter__.
 */
AccessorMirror.prototype.getter = function(details) {
  return MakeMirror(this.getter_);
}


/**
 * Returns a mirror for the function of a non native setter.
 * @return {FunctionMirror} Function mirror for the getter set using
 *     __defineSetter__.
 */
AccessorMirror.prototype.setter = function(details) {
  return MakeMirror(this.setter_);
}


const kFrameDetailsFrameIdIndex = 0;
const kFrameDetailsReceiverIndex = 1;
const kFrameDetailsFunctionIndex = 2;
const kFrameDetailsArgumentCountIndex = 3;
const kFrameDetailsLocalCountIndex = 4;
const kFrameDetailsSourcePositionIndex = 5;
const kFrameDetailsConstructCallIndex = 6;
const kFrameDetailsDebuggerFrameIndex = 7;
const kFrameDetailsFirstDynamicIndex = 8;

const kFrameDetailsNameIndex = 0;
const kFrameDetailsValueIndex = 1;
const kFrameDetailsNameValueSize = 2;

/**
 * Wrapper for the frame details information retreived from the VM. The frame
 * details from the VM is an array with the following content. See runtime.cc
 * Runtime_GetFrameDetails.
 *     0: Id
 *     1: Receiver
 *     2: Function
 *     3: Argument count
 *     4: Local count
 *     5: Source position
 *     6: Construct call
 *     Arguments name, value
 *     Locals name, value
 * @param {number} break_id Current break id
 * @param {number} index Frame number
 * @constructor
 */
function FrameDetails(break_id, index) {
  this.break_id_ = break_id;
  this.details_ = %GetFrameDetails(break_id, index);
}


FrameDetails.prototype.frameId = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsFrameIdIndex];
}


FrameDetails.prototype.receiver = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsReceiverIndex];
}


FrameDetails.prototype.func = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsFunctionIndex];
}


FrameDetails.prototype.isConstructCall = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsConstructCallIndex];
}


FrameDetails.prototype.isDebuggerFrame = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsDebuggerFrameIndex];
}


FrameDetails.prototype.argumentCount = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsArgumentCountIndex];
}


FrameDetails.prototype.argumentName = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.argumentCount()) {
    return this.details_[kFrameDetailsFirstDynamicIndex +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsNameIndex]
  }
}


FrameDetails.prototype.argumentValue = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.argumentCount()) {
    return this.details_[kFrameDetailsFirstDynamicIndex +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsValueIndex]
  }
}


FrameDetails.prototype.localCount = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsLocalCountIndex];
}


FrameDetails.prototype.sourcePosition = function() {
  %CheckExecutionState(this.break_id_);
  return this.details_[kFrameDetailsSourcePositionIndex];
}


FrameDetails.prototype.localName = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.localCount()) {
    var locals_offset = kFrameDetailsFirstDynamicIndex + this.argumentCount() * kFrameDetailsNameValueSize
    return this.details_[locals_offset +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsNameIndex]
  }
}


FrameDetails.prototype.localValue = function(index) {
  %CheckExecutionState(this.break_id_);
  if (index >= 0 && index < this.localCount()) {
    var locals_offset = kFrameDetailsFirstDynamicIndex + this.argumentCount() * kFrameDetailsNameValueSize
    return this.details_[locals_offset +
                         index * kFrameDetailsNameValueSize +
                         kFrameDetailsValueIndex]
  }
}


/**
 * Mirror object for stack frames.
 * @param {number} break_id The break id in the VM for which this frame is
       valid
 * @param {number} index The frame index (top frame is index 0)
 * @constructor
 * @extends Mirror
 */
function FrameMirror(break_id, index) {
  Mirror.call(this, FRAME_TYPE);
  this.break_id_ = break_id;
  this.index_ = index;
  this.details_ = new FrameDetails(break_id, index);
}
inherits(FrameMirror, Mirror);


FrameMirror.prototype.index = function() {
  return this.index_;
};


FrameMirror.prototype.func = function() {
  // Get the function for this frame from the VM.
  var f = this.details_.func();
  
  // Create a function mirror. NOTE: MakeMirror cannot be used here as the
  // value returned from the VM might be a string if the function for the
  // frame is unresolved.
  if (IS_FUNCTION(f)) {
    return new FunctionMirror(f);
  } else {
    return new UnresolvedFunctionMirror(f);
  }
};


FrameMirror.prototype.receiver = function() {
  return MakeMirror(this.details_.receiver());
};


FrameMirror.prototype.isConstructCall = function() {
  return this.details_.isConstructCall();
};


FrameMirror.prototype.isDebuggerFrame = function() {
  return this.details_.isDebuggerFrame();
};


FrameMirror.prototype.argumentCount = function() {
  return this.details_.argumentCount();
};


FrameMirror.prototype.argumentName = function(index) {
  return this.details_.argumentName(index);
};


FrameMirror.prototype.argumentValue = function(index) {
  return MakeMirror(this.details_.argumentValue(index));
};


FrameMirror.prototype.localCount = function() {
  return this.details_.localCount();
};


FrameMirror.prototype.localName = function(index) {
  return this.details_.localName(index);
};


FrameMirror.prototype.localValue = function(index) {
  return MakeMirror(this.details_.localValue(index));
};


FrameMirror.prototype.sourcePosition = function() {
  return this.details_.sourcePosition();
};


FrameMirror.prototype.sourceLocation = function() {
  if (this.func().resolved() && this.func().script()) {
    return this.func().script().locationFromPosition(this.sourcePosition());
  }
};


FrameMirror.prototype.sourceLine = function() {
  if (this.func().resolved()) {
    var location = this.sourceLocation();
    if (location) {
      return location.line;
    }
  }
};


FrameMirror.prototype.sourceColumn = function() {
  if (this.func().resolved()) {
    var location = this.sourceLocation();
    if (location) {
      return location.column;
    }
  }
};


FrameMirror.prototype.sourceLineText = function() {
  if (this.func().resolved()) {
    var location = this.sourceLocation();
    if (location) {
      return location.sourceText();
    }
  }
};


FrameMirror.prototype.evaluate = function(source, disable_break) {
  var result = %DebugEvaluate(this.break_id_, this.details_.frameId(),
                              source, Boolean(disable_break));
  return MakeMirror(result);
};


FrameMirror.prototype.invocationText = function() {
  // Format frame invoaction (receiver, function and arguments).
  var result = '';
  var func = this.func();
  var receiver = this.receiver();
  if (this.isConstructCall()) {
    // For constructor frames display new followed by the function name.
    result += 'new ';
    result += func.name() ? func.name() : '[anonymous]';
  } else if (this.isDebuggerFrame()) {
    result += '[debugger]';
  } else {
    // If the receiver has a className which is 'global' don't display it.
    var display_receiver = !receiver.className || receiver.className() != 'global';
    if (display_receiver) {
      result += receiver.toText();
    }
    // Try to find the function as a property in the receiver. Include the
    // prototype chain in the lookup.
    var property = new UndefinedMirror();
    if (!receiver.isUndefined()) {
      for (var r = receiver; !r.isNull() && property.isUndefined(); r = r.protoObject()) {
        property = r.lookupProperty(func);
      }
    }
    if (!property.isUndefined()) {
      // The function invoked was found on the receiver. Use the property name
      // for the backtrace.
      if (!property.isIndexed()) {
        if (display_receiver) {
          result += '.';
        }
        result += property.name();
      } else {
        result += '[';
        result += property.name();
        result += ']';
      }
      // Also known as - if the name in the function doesn't match the name
      // under which it was looked up.
      if (func.name() && func.name() != property.name()) {
        result += '(aka ' + func.name() + ')';
      }
    } else {
      // The function invoked was not found on the receiver. Use the function
      // name if available for the backtrace.
      if (display_receiver) {
        result += '.';
      }
      result += func.name() ? func.name() : '[anonymous]';
    }
  }

  // Render arguments for normal frames.
  if (!this.isDebuggerFrame()) {
    result += '(';
    for (var i = 0; i < this.argumentCount(); i++) {
      if (i != 0) result += ', ';
      if (this.argumentName(i)) {
        result += this.argumentName(i);
        result += '=';
      }
      result += this.argumentValue(i).toText();
    }
    result += ')';
  }

  return result;
}


FrameMirror.prototype.sourceAndPositionText = function() {
  // Format source and position.
  var result = '';
  var func = this.func();
  if (func.resolved()) {
    if (func.script()) {
      if (func.script().name()) {
        result += func.script().name();
      } else {
        result += '[unnamed]';
      }
      if (!this.isDebuggerFrame()) {
        var location = this.sourceLocation();
        result += ' line ';
        result += !IS_UNDEFINED(location) ? (location.line + 1) : '?';
        result += ' column ';
        result += !IS_UNDEFINED(location) ? (location.column + 1) : '?';
        if (!IS_UNDEFINED(this.sourcePosition())) {
          result += ' (position ' + (this.sourcePosition() + 1) + ')';
        }
      }
    } else {
      result += '[no source]';
    }
  } else {
    result += '[unresolved]';
  }

  return result;
}


FrameMirror.prototype.localsText = function() {
  // Format local variables.
  var result = '';
  var locals_count = this.localCount()
  if (locals_count > 0) {
    for (var i = 0; i < locals_count; ++i) {
      result += '      var ';
      result += this.localName(i);
      result += ' = ';
      result += this.localValue(i).toText();
      if (i < locals_count - 1) result += '\n';
    }
  }

  return result;
}


FrameMirror.prototype.toText = function(opt_locals) {
  var result = '';
  result += '#' + (this.index() <= 9 ? '0' : '') + this.index();
  result += ' ';
  result += this.invocationText();
  result += ' ';
  result += this.sourceAndPositionText();
  if (opt_locals) {
    result += '\n';
    result += this.localsText();
  }
  return result;
}


/**
 * Mirror object for script source.
 * @param {Script} script The script object
 * @constructor
 * @extends Mirror
 */
function ScriptMirror(script) {
  Mirror.call(this, SCRIPT_TYPE);
  this.script_ = script;
}
inherits(ScriptMirror, Mirror);


ScriptMirror.prototype.name = function() {
  return this.script_.name;
};


ScriptMirror.prototype.lineOffset = function() {
  return this.script_.line_offset;
};


ScriptMirror.prototype.columnOffset = function() {
  return this.script_.column_offset;
};


ScriptMirror.prototype.scriptType = function() {
  return this.script_.type;
};


ScriptMirror.prototype.lineCount = function() {
  return this.script_.lineCount();
};


ScriptMirror.prototype.locationFromPosition = function(position) {
  return this.script_.locationFromPosition(position);
}


ScriptMirror.prototype.sourceSlice = function (opt_from_line, opt_to_line) {
  return this.script_.sourceSlice(opt_from_line, opt_to_line);
}


ScriptMirror.prototype.toText = function() {
  var result = '';
  result += this.name();
  result += ' (lines: ';
  if (this.lineOffset() > 0) {
    result += this.lineOffset();
    result += '-';
    result += this.lineOffset() + this.lineCount() - 1;
  } else {
    result += this.lineCount();
  }
  result += ')';
  return result;
}


function JSONProtocolSerializer(details) {
  this.details_ = details;
}


JSONProtocolSerializer.prototype.serialize = function(mirror) {
  // Collect the JSON property/value pairs in a array.
  var content = new Array();

  // Always add the type  
  content.push(MakeJSONPair_('type', StringToJSON_(mirror.type())));

  switch (mirror.type()) {
    case UNDEFINED_TYPE:
    case NULL_TYPE:
      // Undefined and null are represented just by their type.
      break;

    case BOOLEAN_TYPE:
      // Boolean values are simply represented by their value.
      content.push(MakeJSONPair_('value', BooleanToJSON_(mirror.value())));
      break;

    case NUMBER_TYPE:
      // Number values are simply represented by their value.
      content.push(MakeJSONPair_('value', NumberToJSON_(mirror.value())));
      break;

    case STRING_TYPE:
      // String values might have their value cropped to keep down size.
      if (mirror.length() > kMaxProtocolStringLength) {
        var str = mirror.value().substring(0, kMaxProtocolStringLength);
        content.push(MakeJSONPair_('value', StringToJSON_(str)));
        content.push(MakeJSONPair_('fromIndex', NumberToJSON_(0)));
        content.push(MakeJSONPair_('toIndex',
                                   NumberToJSON_(kMaxProtocolStringLength)));
      } else {
        content.push(MakeJSONPair_('value', StringToJSON_(mirror.value())));
      }
      content.push(MakeJSONPair_('length', NumberToJSON_(mirror.length())));
      break;

    case OBJECT_TYPE:
    case FUNCTION_TYPE:
    case ERROR_TYPE:
    case REGEXP_TYPE:
      // Add object representation.
      this.serializeObject_(mirror, content);
      break;

    case PROPERTY_TYPE:
      // Properties are represented by name, value, attributes and type.
      content.push(MakeJSONPair_('name',
                                 StringToJSON_(mirror.name())));
      content.push(MakeJSONPair_('value',
                                 mirror.value().toJSONProtocol(this.details_)));
      if (mirror.attributes() != PropertyAttribute.None) {
        content.push(MakeJSONPair_('attributes',
                                   NumberToJSON_(mirror.attributes())));
      }
      if (mirror.propertyType() != PropertyType.Normal) {
        content.push(MakeJSONPair_('propertyType',
                                   NumberToJSON_(mirror.propertyType())));
      }
      break;

    case ACCESSOR_TYPE:
      // An accessor can either be native or defined through JavaScript.
      if (mirror.isNative()) {
        content.push(MakeJSONPair_('native', BooleanToJSON_(true)));
      } else {
        content.push(MakeJSONPair_('getter',
                                   mirror.getter().toJSONProtocol(false)));
        content.push(MakeJSONPair_('setter',
                                   mirror.setter().toJSONProtocol(false)));
      }
      break;

    case FRAME_TYPE:
      // Add object representation.
      this.serializeFrame_(mirror, content);
      break;

    case SCRIPT_TYPE:
      // Script is represented by name and source attributes.
      if (mirror.name()) {
        content.push(MakeJSONPair_('name', StringToJSON_(mirror.name())));
      }
      content.push(MakeJSONPair_('lineOffset',
                                 NumberToJSON_(mirror.lineOffset())));
      content.push(MakeJSONPair_('columnOffset',
                                 NumberToJSON_(mirror.columnOffset())));
      content.push(MakeJSONPair_('lineCount',
                                 NumberToJSON_(mirror.lineCount())));
      content.push(MakeJSONPair_('scriptType',
                                 NumberToJSON_(mirror.scriptType())));
      break;

  }

  // Always add the text representation.
  content.push(MakeJSONPair_('text', StringToJSON_(mirror.toText())));
  
  // Create and return the JSON string.
  return ArrayToJSONObject_(content);
}


JSONProtocolSerializer.prototype.serializeObject_ = function(mirror, content) {
  content.push(MakeJSONPair_('className',
                             StringToJSON_(mirror.className())));

  if (this.details_) {
    content.push(MakeJSONPair_('constructorFunction',
        mirror.constructorFunction().toJSONProtocol(false)));
    content.push(MakeJSONPair_('protoObject',
                               mirror.protoObject().toJSONProtocol(false)));
    content.push(MakeJSONPair_('prototypeObject',
                               mirror.prototypeObject().toJSONProtocol(false)));

    // Add properties. For arrays don't include indexed proeprties.
    var kind = PropertyKind.Named;
    if (!mirror.isArray()) {
      kind |= PropertyKind.Indexed
    }
    var propertyNames = mirror.propertyNames(kind);
    var x = new Array(propertyNames.length);
    for (var i = 0; i < propertyNames.length; i++) {
      x[i] = mirror.property(propertyNames[i]).toJSONProtocol(false);
    }
    content.push(MakeJSONPair_('properties', ArrayToJSONArray_(x)));

    // Add interceptor properties.
    propertyNames = mirror.interceptorPropertyNames();
    var x = new Array(propertyNames.length);
    for (var i = 0; i < propertyNames.length; i++) {
      x[i] = properties[i].toJSONProtocol(details);
    }
    content.push(MakeJSONPair_('interceptorProperties', ArrayToJSONArray_(x)));

    // For arrays the indexed properties are added separately and the length is
    // added as well.
    if (mirror.isArray()) {
      var propertyNames = mirror.propertyNames(PropertyKind.Indexed);
      var x = new Array(propertyNames.length);
      for (var i = 0; i < propertyNames.length; i++) {
        x[i] = mirror.property(propertyNames[i]).toJSONProtocol(false);
      }
      content.push(MakeJSONPair_('indexedProperties', ArrayToJSONArray_(x)));

      // Add the array length.
      content.push(MakeJSONPair_('length', NumberToJSON_(mirror.length())));
    }
  }

  if (mirror.hasNamedInterceptor()) {
    content.push(MakeJSONPair_('namedInterceptor', BooleanToJSON_(true)));
  }

  if (mirror.hasIndexedInterceptor()) {
    content.push(MakeJSONPair_('indexedInterceptor', BooleanToJSON_(true)));
  }
  
  if (mirror.isFunction()) {
    // Add function specific properties.
    content.push(MakeJSONPair_('name', StringToJSON_(mirror.name())));
    content.push(MakeJSONPair_('resolved', BooleanToJSON_(mirror.resolved())));
    if (this.details_ && mirror.resolved()) {
      content.push(MakeJSONPair_('source', StringToJSON_(mirror.source())));
    }
    if (mirror.script()) {
      content.push(MakeJSONPair_('script', mirror.script().toJSONProtocol()));
    }
  } else if (mirror.isDate()) {
    // Add date specific properties.
    content.push(MakeJSONPair_('value', DateToJSON_(mirror.value())));
  } else if (mirror.isRegExp()) {
    // Add regexp specific properties.
    content.push(MakeJSONPair_('source', StringToJSON_(mirror.source())));
    content.push(MakeJSONPair_('global', BooleanToJSON_(mirror.global())));
    content.push(MakeJSONPair_('ignoreCase',
                               BooleanToJSON_(mirror.ignoreCase())));
    content.push(MakeJSONPair_('multiline',
                               BooleanToJSON_(mirror.multiline())));
  } else if (mirror.isError()) {
    // Add error specific properties.
    content.push(MakeJSONPair_('message', StringToJSON_(mirror.message())));
  }
}


JSONProtocolSerializer.prototype.serializeFrame_ = function(mirror, content) {
  content.push(MakeJSONPair_('index', NumberToJSON_(mirror.index())));
  content.push(MakeJSONPair_('receiver',
                             mirror.receiver().toJSONProtocol(false)));
  content.push(MakeJSONPair_('func', mirror.func().toJSONProtocol(false)));
  content.push(MakeJSONPair_('constructCall',
                             BooleanToJSON_(mirror.isConstructCall())));
  content.push(MakeJSONPair_('debuggerFrame',
                             BooleanToJSON_(mirror.isDebuggerFrame())));
  var x = new Array(mirror.argumentCount());
  for (var i = 0; i < mirror.argumentCount(); i++) {
    arg = new Array();
    var argument_name = mirror.argumentName(i)
    if (argument_name) {
      arg.push(MakeJSONPair_('name', StringToJSON_(argument_name)));
    }
    arg.push(MakeJSONPair_('value',
                           mirror.argumentValue(i).toJSONProtocol(false)));
    x[i] = ArrayToJSONObject_(arg);
  }
  content.push(MakeJSONPair_('arguments', ArrayToJSONArray_(x)));
  var x = new Array(mirror.localCount());
  for (var i = 0; i < mirror.localCount(); i++) {
    var name = MakeJSONPair_('name', StringToJSON_(mirror.localName(i)));
    var value = MakeJSONPair_('value',
                              mirror.localValue(i).toJSONProtocol(false));
    x[i] = '{' + name + ',' + value + '}';
  }
  content.push(MakeJSONPair_('locals', ArrayToJSONArray_(x)));
  content.push(MakeJSONPair_('position',
                             NumberToJSON_(mirror.sourcePosition())));
  var line = mirror.sourceLine();
  if (!IS_UNDEFINED(line)) {
    content.push(MakeJSONPair_('line', NumberToJSON_(line)));
  }
  var column = mirror.sourceColumn();
  if (!IS_UNDEFINED(column)) {
    content.push(MakeJSONPair_('column', NumberToJSON_(column)));
  }
  var source_line_text = mirror.sourceLineText();
  if (!IS_UNDEFINED(source_line_text)) {
    content.push(MakeJSONPair_('sourceLineText',
                               StringToJSON_(source_line_text)));
  }
}


function MakeJSONPair_(name, value) {
  return '"' + name + '":' + value;
}


function ArrayToJSONObject_(content) {
  return '{' + content.join(',') + '}';
}


function ArrayToJSONArray_(content) {
  return '[' + content.join(',') + ']';
}


function BooleanToJSON_(value) {
  return String(value); 
}


function NumberToJSON_(value) {
  return String(value); 
}


// Mapping of some control characters to avoid the \uXXXX syntax for most
// commonly used control cahracters.
const ctrlCharMap_ = {
  '\b': '\\b',
  '\t': '\\t',
  '\n': '\\n',
  '\f': '\\f',
  '\r': '\\r',
  '"' : '\\"',
  '\\': '\\\\'
};


// Regular expression testing for ", \ and control characters (0x00 - 0x1F).
const ctrlCharTest_ = new RegExp('["\\\\\x00-\x1F]');


// Regular expression matching ", \ and control characters (0x00 - 0x1F)
// globally.
const ctrlCharMatch_ = new RegExp('["\\\\\x00-\x1F]', 'g');


/**
 * Convert a String to its JSON representation (see http://www.json.org/). To
 * avoid depending on the String object this method calls the functions in
 * string.js directly and not through the value.
 * @param {String} value The String value to format as JSON
 * @return {string} JSON formatted String value
 */
function StringToJSON_(value) {
  // Check for" , \ and control characters (0x00 - 0x1F). No need to call
  // RegExpTest as ctrlchar is constructed using RegExp.
  if (ctrlCharTest_.test(value)) {
    // Replace ", \ and control characters (0x00 - 0x1F).
    return '"' +
      value.replace(ctrlCharMatch_, function (char) {
        // Use charmap if possible.
        var mapped = ctrlCharMap_[char];
        if (mapped) return mapped;
        mapped = char.charCodeAt();
        // Convert control character to unicode escape sequence.
        return '\\u00' +
          %NumberToRadixString(Math.floor(mapped / 16), 16) +
          %NumberToRadixString(mapped % 16, 16);
      })
    + '"';
  }

  // Simple string with no special characters.
  return '"' + value + '"';
}


/**
 * Convert a Date to ISO 8601 format. To avoid depending on the Date object
 * this method calls the functions in date.js directly and not through the
 * value.
 * @param {Date} value The Date value to format as JSON
 * @return {string} JSON formatted Date value
 */
function DateToISO8601_(value) {
  function f(n) {
    return n < 10 ? '0' + n : n;
  }
  function g(n) {
    return n < 10 ? '00' + n : n < 100 ? '0' + n : n;
  }
  return builtins.GetUTCFullYearFrom(value)         + '-' +
          f(builtins.GetUTCMonthFrom(value) + 1)    + '-' +
          f(builtins.GetUTCDateFrom(value))         + 'T' +
          f(builtins.GetUTCHoursFrom(value))        + ':' +
          f(builtins.GetUTCMinutesFrom(value))      + ':' +
          f(builtins.GetUTCSecondsFrom(value))      + '.' +
          g(builtins.GetUTCMillisecondsFrom(value)) + 'Z';
}

/**
 * Convert a Date to ISO 8601 format. To avoid depending on the Date object
 * this method calls the functions in date.js directly and not through the
 * value.
 * @param {Date} value The Date value to format as JSON
 * @return {string} JSON formatted Date value
 */
function DateToJSON_(value) {
  return '"' + DateToISO8601_(value) + '"';
}
