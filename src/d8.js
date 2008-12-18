// Copyright 2008 the V8 project authors. All rights reserved.
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

// How crappy is it that I have to implement completely basic stuff
// like this myself?  Answer: very.
String.prototype.startsWith = function (str) {
  if (str.length > this.length)
    return false;
  return this.substr(0, str.length) == str;
}

function log10(num) {
  return Math.log(num)/Math.log(10);
}

function ToInspectableObject(obj) {
  if (!obj && typeof obj === 'object') {
    return void 0;
  } else {
    return Object(obj);
  }
}

function GetCompletions(global, last, full) {
  var full_tokens = full.split();
  full = full_tokens.pop();
  var parts = full.split('.');
  parts.pop();
  var current = global;
  for (var i = 0; i < parts.length; i++) {
    var part = parts[i];
    var next = current[part];
    if (!next)
      return [];
    current = next;
  }
  var result = [];
  current = ToInspectableObject(current);
  while (typeof current !== 'undefined') {
    var mirror = new $debug.ObjectMirror(current);
    var properties = mirror.properties();
    for (var i = 0; i < properties.length; i++) {
      var name = properties[i].name();
      if (typeof name === 'string' && name.startsWith(last))
        result.push(name);
    }
    current = ToInspectableObject(current.__proto__);
  }
  return result;
}


// Global object holding debugger related constants and state.
const Debug = {};


// Debug events which can occour in the V8 JavaScript engine. These originate
// from the API include file debug.h.
Debug.DebugEvent = { Break: 1,
                     Exception: 2,
                     NewFunction: 3,
                     BeforeCompile: 4,
                     AfterCompile: 5 };


// The different types of scripts matching enum ScriptType in objects.h.
Debug.ScriptType = { Native: 0,
                     Extension: 1,
                     Normal: 2 };


// Current debug state.
const kNoFrame = -1;
Debug.State = {
  currentFrame: kNoFrame,
  currentSourceLine: -1
}


function DebugEventToText(event) {
  if (event.eventType() == 1) {
    // Build the break details.
    var details = '';
    if (event.breakPointsHit()) {
      details += 'breakpoint';
      if (event.breakPointsHit().length > 1) {
        details += 's';
      }
      details += ' #';
      for (var i = 0; i < event.breakPointsHit().length; i++) {
        if (i > 0) {
          details += ', #';
        }
        // Find the break point number. For break points originating from a
        // script break point display the script break point number.
        var break_point = event.breakPointsHit()[i];
        var script_break_point = break_point.script_break_point();
        if (script_break_point) {
          details += script_break_point.number();
        } else {
          details += break_point.number();
        }
      }
    } else {
      details += 'break';
    }
    details += ' in ';
    details += event.executionState().frame(0).invocationText();
    details += ' at ';
    details += event.executionState().frame(0).sourceAndPositionText();
    details += '\n'
    if (event.func().script()) {
      details += FrameSourceUnderline(event.executionState().frame(0));
    }
    Debug.State.currentSourceLine =
        event.executionState().frame(0).sourceLine();
    Debug.State.currentFrame = 0;
    return details;
  } else if (event.eventType() == 2) {
    var details = '';
    if (event.uncaught_) {
      details += 'Uncaught: ';
    } else {
      details += 'Exception: ';
    }

    details += '"';
    details += event.exception();
    details += '"';
    if (event.executionState().frameCount() > 0) {
      details += '"';
      details += event.exception();
      details += ' at ';
      details += event.executionState().frame(0).sourceAndPositionText();
      details += '\n';
      details += FrameSourceUnderline(event.executionState().frame(0));
      Debug.State.currentSourceLine =
          event.executionState().frame(0).sourceLine();
      Debug.State.currentFrame = 0;
    } else {
      details += ' (empty stack)';
      Debug.State.currentSourceLine = -1;
      Debug.State.currentFrame = kNoFrame;
    }

    return details;
  }

  return 'Unknown debug event ' + event.eventType();
};


function SourceUnderline(source_text, position) {
  if (!source_text) {
    return;
  }

  // Create an underline with a caret pointing to the source position. If the		
  // source contains a tab character the underline will have a tab character in		
  // the same place otherwise the underline will have a space character.		
  var underline = '';
  for (var i = 0; i < position; i++) {
    if (source_text[i] == '\t') {
      underline += '\t';
    } else {
      underline += ' ';
    }
  }
  underline += '^';

  // Return the source line text with the underline beneath.
  return source_text + '\n' + underline;
};


function FrameSourceUnderline(frame) {		
  var location = frame.sourceLocation();
  if (location) {
    return SourceUnderline(location.sourceText(),
                           location.position - location.start);
  }
};


// Converts a text command to a JSON request.
function DebugCommandToJSONRequest(cmd_line) {
  return new DebugRequest(cmd_line).JSONRequest();
};


function DebugRequest(cmd_line) {
  // If the very first character is a { assume that a JSON request have been
  // entered as a command. Converting that to a JSON request is trivial.
  if (cmd_line && cmd_line.length > 0 && cmd_line.charAt(0) == '{') {
    this.request_ = cmd_line;
    return;
  }

  // Trim string for leading and trailing whitespace.
  cmd_line = cmd_line.replace(/^\s+|\s+$/g, '');

  // Find the command.
  var pos = cmd_line.indexOf(' ');
  var cmd;
  var args;
  if (pos == -1) {
    cmd = cmd_line;
    args = '';
  } else {
    cmd = cmd_line.slice(0, pos);
    args = cmd_line.slice(pos).replace(/^\s+|\s+$/g, '');
  }

  // Switch on command.
  switch (cmd) {
    case 'continue':
    case 'c':
      this.request_ = this.continueCommandToJSONRequest_(args);
      break;

    case 'step':
    case 's':
      this.request_ = this.stepCommandToJSONRequest_(args);
      break;

    case 'backtrace':
    case 'bt':
      this.request_ = this.backtraceCommandToJSONRequest_(args);
      break;
      
    case 'frame':
    case 'f':
      this.request_ = this.frameCommandToJSONRequest_(args);
      break;
      
    case 'print':
    case 'p':
      this.request_ = this.printCommandToJSONRequest_(args);
      break;

    case 'source':
      this.request_ = this.sourceCommandToJSONRequest_(args);
      break;
      
    case 'scripts':
      this.request_ = this.scriptsCommandToJSONRequest_(args);
      break;
      
    case 'break':
    case 'b':
      this.request_ = this.breakCommandToJSONRequest_(args);
      break;
      
    case 'clear':
      this.request_ = this.clearCommandToJSONRequest_(args);
      break;

    case 'help':
    case '?':
      this.helpCommand_(args);
      // Return null to indicate no JSON to send (command handled internally). 
      this.request_ = void 0;  
      break;

    default:
      throw new Error('Unknown command "' + cmd + '"');
  }
}

DebugRequest.prototype.JSONRequest = function() {
  return this.request_;
}


function RequestPacket(command) {
  this.seq = 0;
  this.type = 'request';
  this.command = command;
}


RequestPacket.prototype.toJSONProtocol = function() {
  // Encode the protocol header.
  var json = '{';
  json += '"seq":' + this.seq;
  json += ',"type":"' + this.type + '"';
  if (this.command) {
    json += ',"command":' + StringToJSON_(this.command);
  }
  if (this.arguments) {
    json += ',"arguments":';
    // Encode the arguments part.
    if (this.arguments.toJSONProtocol) {
      json += this.arguments.toJSONProtocol()
    } else {
      json += SimpleObjectToJSON_(this.arguments);
    }
  }
  json += '}';
  return json;
}


DebugRequest.prototype.createRequest = function(command) {
  return new RequestPacket(command);
};


// Create a JSON request for the continue command.
DebugRequest.prototype.continueCommandToJSONRequest_ = function(args) {
  var request = this.createRequest('continue');
  return request.toJSONProtocol();
};


// Create a JSON request for the step command.
DebugRequest.prototype.stepCommandToJSONRequest_ = function(args) {
  // Requesting a step is through the continue command with additional
  // arguments.
  var request = this.createRequest('continue');
  request.arguments = {};

  // Process arguments if any.
  if (args && args.length > 0) {
    args = args.split(/\s*[ ]+\s*/g);

    if (args.length > 2) {
      throw new Error('Invalid step arguments.');
    }

    if (args.length > 0) {
      // Get step count argument if any.
      if (args.length == 2) {
        var stepcount = parseInt(args[1]);
        if (isNaN(stepcount) || stepcount <= 0) {
          throw new Error('Invalid step count argument "' + args[0] + '".');
        }
        request.arguments.stepcount = stepcount;
      }

      // Get the step action.
      switch (args[0]) {
        case 'in':
        case 'i':
          request.arguments.stepaction = 'in';
          break;
          
        case 'min':
        case 'm':
          request.arguments.stepaction = 'min';
          break;
          
        case 'next':
        case 'n':
          request.arguments.stepaction = 'next';
          break;
          
        case 'out':
        case 'o':
          request.arguments.stepaction = 'out';
          break;
          
        default:
          throw new Error('Invalid step argument "' + args[0] + '".');
      }
    }
  } else {
    // Default is step next.
    request.arguments.stepaction = 'next';
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the backtrace command.
DebugRequest.prototype.backtraceCommandToJSONRequest_ = function(args) {
  // Build a backtrace request from the text command.
  var request = this.createRequest('backtrace');
  args = args.split(/\s*[ ]+\s*/g);
  if (args.length == 2) {
    request.arguments = {};
    var fromFrame = parseInt(args[0]);
    var toFrame = parseInt(args[1]);
    if (isNaN(fromFrame) || fromFrame < 0) {
      throw new Error('Invalid start frame argument "' + args[0] + '".');
    }
    if (isNaN(toFrame) || toFrame < 0) {
      throw new Error('Invalid end frame argument "' + args[1] + '".');
    }
    if (fromFrame > toFrame) {
      throw new Error('Invalid arguments start frame cannot be larger ' +
                      'than end frame.');
    }
    request.arguments.fromFrame = fromFrame;
    request.arguments.toFrame = toFrame + 1;
  }
  return request.toJSONProtocol();
};


// Create a JSON request for the frame command.
DebugRequest.prototype.frameCommandToJSONRequest_ = function(args) {
  // Build a frame request from the text command.
  var request = this.createRequest('frame');
  args = args.split(/\s*[ ]+\s*/g);
  if (args.length > 0 && args[0].length > 0) {
    request.arguments = {};
    request.arguments.number = args[0];
  }
  return request.toJSONProtocol();
};


// Create a JSON request for the print command.
DebugRequest.prototype.printCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('evaluate');
  if (args.length == 0) {
    throw new Error('Missing expression.');
  }

  request.arguments = {};
  request.arguments.expression = args;

  return request.toJSONProtocol();
};


// Create a JSON request for the source command.
DebugRequest.prototype.sourceCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('source');

  // Default is ten lines starting five lines before the current location.
  var from = Debug.State.currentSourceLine - 5;
  var lines = 10;

  // Parse the arguments.
  args = args.split(/\s*[ ]+\s*/g);
  if (args.length > 1 && args[0].length > 0 && args[1].length > 0) {
    from = parseInt(args[0]) - 1;
    lines = parseInt(args[1]);
  } else if (args.length > 0 && args[0].length > 0) {
    from = parseInt(args[0]) - 1;
  }

  if (from < 0) from = 0;
  if (lines < 0) lines = 10;

  // Request source arround current source location.
  request.arguments = {};
  request.arguments.fromLine = from;
  request.arguments.toLine = from + lines;

  return request.toJSONProtocol();
};


// Create a JSON request for the scripts command.
DebugRequest.prototype.scriptsCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('scripts');

  // Process arguments if any.
  if (args && args.length > 0) {
    args = args.split(/\s*[ ]+\s*/g);

    if (args.length > 1) {
      throw new Error('Invalid scripts arguments.');
    }

    request.arguments = {};
    switch (args[0]) {
      case 'natives':
        request.arguments.types = ScriptTypeFlag(Debug.ScriptType.Native);
        break;
        
      case 'extensions':
        request.arguments.types = ScriptTypeFlag(Debug.ScriptType.Extension);
        break;
        
      case 'all':
        request.arguments.types =
            ScriptTypeFlag(Debug.ScriptType.Normal) |
            ScriptTypeFlag(Debug.ScriptType.Native) |
            ScriptTypeFlag(Debug.ScriptType.Extension);
        break;
        
      default:
        throw new Error('Invalid argument "' + args[0] + '".');
    }
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the break command.
DebugRequest.prototype.breakCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('setbreakpoint');

  // Process arguments if any.
  if (args && args.length > 0) {
    var target = args;
    var condition;

    var pos = args.indexOf(' ');
    if (pos > 0) {
      target = args.substring(0, pos);
      condition = args.substring(pos + 1, args.length);
    }

    request.arguments = {};
    request.arguments.type = 'function';
    request.arguments.target = target;
    request.arguments.condition = condition;
  } else {
    throw new Error('Invalid break arguments.');
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the clear command.
DebugRequest.prototype.clearCommandToJSONRequest_ = function(args) {
  // Build a evaluate request from the text command.
  var request = this.createRequest('clearbreakpoint');

  // Process arguments if any.
  if (args && args.length > 0) {
    request.arguments = {};
    request.arguments.breakpoint = parseInt(args);
  } else {
    throw new Error('Invalid break arguments.');
  }

  return request.toJSONProtocol();
};


// Create a JSON request for the break command.
DebugRequest.prototype.helpCommand_ = function(args) {
  // Help os quite simple.
  if (args && args.length > 0) {
    print('warning: arguments to \'help\' are ignored');
  }

  print('break location [condition]');
  print('clear <breakpoint #>');
  print('backtrace [from frame #] [to frame #]]');
  print('frame <frame #>');
  print('step [in | next | out| min [step count]]');
  print('print <expression>');
  print('source [from line [num lines]]');
  print('scripts');
  print('continue');
  print('help');
}


// Convert a JSON response to text for display in a text based debugger.
function DebugResponseDetails(json_response) {
  details = {text:'', running:false}

  try {
    // Convert the JSON string to an object.
    response = eval('(' + json_response + ')');

    if (!response.success) {
      details.text = response.message;
      return details;
    }

    // Get the running state.
    details.running = response.running;

    switch (response.command) {
      case 'setbreakpoint':
        var body = response.body;
        result = 'set breakpoint #';
        result += body.breakpoint;
        details.text = result;
        break;
        
      case 'clearbreakpoint':
        var body = response.body;
        result = 'cleared breakpoint #';
        result += body.breakpoint;
        details.text = result;
        break;
        
      case 'backtrace':
        var body = response.body;
        if (body.totalFrames == 0) {
          result = '(empty stack)';
        } else {
          var result = 'Frames #' + body.fromFrame + ' to #' +
              (body.toFrame - 1) + ' of ' + body.totalFrames + '\n';
          for (i = 0; i < body.frames.length; i++) {
            if (i != 0) result += '\n';
            result += body.frames[i].text;
          }
        }
        details.text = result;
        break;
        
      case 'frame':
        details.text = SourceUnderline(response.body.sourceLineText,
                                       response.body.column);
        Debug.State.currentSourceLine = response.body.line;
        Debug.State.currentFrame = response.body.index;
        break;
        
      case 'evaluate':
        details.text =  response.body.text;
        break;
        
      case 'source':
        // Get the source from the response.
        var source = response.body.source;
        var from_line = response.body.fromLine + 1;
        var lines = source.split('\n');
        var maxdigits = 1 + Math.floor(log10(from_line + lines.length));
        if (maxdigits < 3) {
          maxdigits = 3;
        }
        var result = '';
        for (var num = 0; num < lines.length; num++) {
          // Check if there's an extra newline at the end.
          if (num == (lines.length - 1) && lines[num].length == 0) {
            break;
          }

          var current_line = from_line + num;
          spacer = maxdigits - (1 + Math.floor(log10(current_line)));
          if (current_line == Debug.State.currentSourceLine + 1) {
            for (var i = 0; i < maxdigits; i++) {
              result += '>';
            }
            result += '  ';
          } else {
            for (var i = 0; i < spacer; i++) {
              result += ' ';
            }
            result += current_line + ': ';
          }
          result += lines[num];
          result += '\n';
        }
        details.text = result;
        break;
        
      case 'scripts':
        var result = '';
        for (i = 0; i < response.body.length; i++) {
          if (i != 0) result += '\n';
          if (response.body[i].name) {
            result += response.body[i].name;
          } else {
            result += '[unnamed] ';
            var sourceStart = response.body[i].sourceStart;
            if (sourceStart.length > 40) {
              sourceStart = sourceStart.substring(0, 37) + '...';
            }
            result += sourceStart;
          }
          result += ' (lines: ';
          result += response.body[i].sourceLines;
          result += ', length: ';
          result += response.body[i].sourceLength;
          if (response.body[i].type == Debug.ScriptType.Native) {
            result += ', native';
          } else if (response.body[i].type == Debug.ScriptType.Extension) {
            result += ', extension';
          }
          result += ')';
        }
        details.text = result;
        break;

      default:
        details.text =
            'Response for unknown command \'' + response.command + '\'';
    }
  } catch (e) {
    details.text = 'Error: "' + e + '" formatting response';
  }
  
  return details;
};


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
          '0' + // TODO %NumberToRadixString(Math.floor(mapped / 16), 16) +
          '0' // TODO %NumberToRadixString(mapped % 16, 16);
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


/**
 * Convert an Object to its JSON representation (see http://www.json.org/).
 * This implementation simply runs through all string property names and adds
 * each property to the JSON representation for some predefined types. For type
 * "object" the function calls itself recursively unless the object has the
 * function property "toJSONProtocol" in which case that is used. This is not
 * a general implementation but sufficient for the debugger. Note that circular
 * structures will cause infinite recursion.
 * @param {Object} object The object to format as JSON
 * @return {string} JSON formatted object value
 */
function SimpleObjectToJSON_(object) {
  var content = [];
  for (var key in object) {
    // Only consider string keys.
    if (typeof key == 'string') {
      var property_value = object[key];

      // Format the value based on its type.
      var property_value_json;
      switch (typeof property_value) {
        case 'object':
          if (typeof property_value.toJSONProtocol == 'function') {
            property_value_json = property_value.toJSONProtocol(true)
          } else if (property_value.constructor.name == 'Array'){
            property_value_json = SimpleArrayToJSON_(property_value);
          } else {
            property_value_json = SimpleObjectToJSON_(property_value);
          }
          break;

        case 'boolean':
          property_value_json = BooleanToJSON_(property_value);
          break;

        case 'number':
          property_value_json = NumberToJSON_(property_value);
          break;

        case 'string':
          property_value_json = StringToJSON_(property_value);
          break;

        default:
          property_value_json = null;
      }

      // Add the property if relevant.
      if (property_value_json) {
        content.push(StringToJSON_(key) + ':' + property_value_json);
      }
    }
  }

  // Make JSON object representation.
  return '{' + content.join(',') + '}';
}


/**
 * Convert an array to its JSON representation. This is a VERY simple
 * implementation just to support what is needed for the debugger.
 * @param {Array} arrya The array to format as JSON
 * @return {string} JSON formatted array value
 */
function SimpleArrayToJSON_(array) {
  // Make JSON array representation.
  var json = '[';
  for (var i = 0; i < array.length; i++) {
    if (i != 0) {
      json += ',';
    }
    var elem = array[i];
    if (elem.toJSONProtocol) {
      json += elem.toJSONProtocol(true)
    } else if (typeof(elem) === 'object')  {
      json += SimpleObjectToJSON_(elem);
    } else if (typeof(elem) === 'boolean')  {
      json += BooleanToJSON_(elem);
    } else if (typeof(elem) === 'number')  {
      json += NumberToJSON_(elem);
    } else if (typeof(elem) === 'string')  {
      json += StringToJSON_(elem);
    } else {
      json += elem;
    }
  }
  json += ']';
  return json;
}
