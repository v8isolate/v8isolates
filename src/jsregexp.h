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

#ifndef V8_JSREGEXP_H_
#define V8_JSREGEXP_H_

namespace v8 { namespace internal {


class RegExpMacroAssembler;


class RegExpImpl {
 public:
  // Creates a regular expression literal in the old space.
  // This function calls the garbage collector if necessary.
  static Handle<Object> CreateRegExpLiteral(Handle<JSFunction> constructor,
                                            Handle<String> pattern,
                                            Handle<String> flags,
                                            bool* has_pending_exception);

  // Returns a string representation of a regular expression.
  // Implements RegExp.prototype.toString, see ECMA-262 section 15.10.6.4.
  // This function calls the garbage collector if necessary.
  static Handle<String> ToString(Handle<Object> value);

  // Parses the RegExp pattern and prepares the JSRegExp object with
  // generic data and choice of implementation - as well as what
  // the implementation wants to store in the data field.
  static Handle<Object> Compile(Handle<JSRegExp> re,
                                Handle<String> pattern,
                                Handle<String> flags);

  // Implements RegExp.prototype.exec(string) function.
  // See ECMA-262 section 15.10.6.2.
  // This function calls the garbage collector if necessary.
  static Handle<Object> Exec(Handle<JSRegExp> regexp,
                             Handle<String> subject,
                             Handle<Object> index);

  // Call RegExp.prototyp.exec(string) in a loop.
  // Used by String.prototype.match and String.prototype.replace.
  // This function calls the garbage collector if necessary.
  static Handle<Object> ExecGlobal(Handle<JSRegExp> regexp,
                                   Handle<String> subject);

  // Stores an uncompiled RegExp pattern in the JSRegExp object.
  // It will be compiled by JSCRE when first executed.
  static Handle<Object> JscrePrepare(Handle<JSRegExp> re,
                                     Handle<String> pattern,
                                     JSRegExp::Flags flags);

  // Prepares a JSRegExp object with Irregexp-specific data.
  static Handle<Object> IrregexpPrepare(Handle<JSRegExp> re,
                                        Handle<String> pattern,
                                        JSRegExp::Flags flags);


  // Compile the pattern using JSCRE and store the result in the
  // JSRegExp object.
  static Handle<Object> JscreCompile(Handle<JSRegExp> re);

  static Handle<Object> AtomCompile(Handle<JSRegExp> re,
                                    Handle<String> pattern,
                                    JSRegExp::Flags flags,
                                    Handle<String> match_pattern);
  static Handle<Object> AtomExec(Handle<JSRegExp> regexp,
                                 Handle<String> subject,
                                 Handle<Object> index);

  static Handle<Object> AtomExecGlobal(Handle<JSRegExp> regexp,
                                       Handle<String> subject);

  static Handle<Object> JscreCompile(Handle<JSRegExp> re,
                                     Handle<String> pattern,
                                     JSRegExp::Flags flags);

  // Execute a compiled JSCRE pattern.
  static Handle<Object> JscreExec(Handle<JSRegExp> regexp,
                                  Handle<String> subject,
                                  Handle<Object> index);

  // Execute an Irregexp bytecode pattern.
  static Handle<Object> IrregexpExec(Handle<JSRegExp> regexp,
                                     Handle<String> subject,
                                     Handle<Object> index);

  static Handle<Object> JscreExecGlobal(Handle<JSRegExp> regexp,
                                        Handle<String> subject);

  static Handle<Object> IrregexpExecGlobal(Handle<JSRegExp> regexp,
                                           Handle<String> subject);

  static void NewSpaceCollectionPrologue();
  static void OldSpaceCollectionPrologue();

  // Converts a source string to a 16 bit flat string.  The string
  // will be either sequential or it will be a SlicedString backed
  // by a flat string.
  static Handle<String> StringToTwoByte(Handle<String> pattern);
  static Handle<String> CachedStringToTwoByte(Handle<String> pattern);

  static const int kIrregexpImplementationIndex = 0;
  static const int kIrregexpNumberOfCapturesIndex = 1;
  static const int kIrregexpNumberOfRegistersIndex = 2;
  static const int kIrregexpCodeIndex = 3;
  static const int kIrregexpDataLength = 4;

  static const int kJscreNumberOfCapturesIndex = 0;
  static const int kJscreInternalIndex = 1;
  static const int kJscreDataLength = 2;

 private:
  static String* last_ascii_string_;
  static String* two_byte_cached_string_;

  static int JscreNumberOfCaptures(Handle<JSRegExp> re);
  static ByteArray* JscreInternal(Handle<JSRegExp> re);

  static int IrregexpNumberOfCaptures(Handle<FixedArray> re);
  static int IrregexpNumberOfRegisters(Handle<FixedArray> re);
  static Handle<ByteArray> IrregexpByteCode(Handle<FixedArray> re);
  static Handle<Code> IrregexpNativeCode(Handle<FixedArray> re);

  // Call jsRegExpExecute once
  static Handle<Object> JscreExecOnce(Handle<JSRegExp> regexp,
                                      int num_captures,
                                      Handle<String> subject,
                                      int previous_index,
                                      const uc16* utf8_subject,
                                      int* ovector,
                                      int ovector_length);

  static Handle<Object> IrregexpExecOnce(Handle<FixedArray> regexp,
                                         int num_captures,
                                         Handle<String> subject16,
                                         int previous_index,
                                         int* ovector,
                                         int ovector_length);

  // Set the subject cache.  The previous string buffer is not deleted, so the
  // caller should ensure that it doesn't leak.
  static void SetSubjectCache(String* subject,
                              char* utf8_subject,
                              int uft8_length,
                              int character_position,
                              int utf8_position);

  // A one element cache of the last utf8_subject string and its length.  The
  // subject JS String object is cached in the heap.  We also cache a
  // translation between position and utf8 position.
  static char* utf8_subject_cache_;
  static int utf8_length_cache_;
  static int utf8_position_;
  static int character_position_;
};


class CharacterRange {
 public:
  CharacterRange() : from_(0), to_(0) { }
  // For compatibility with the CHECK_OK macro
  CharacterRange(void* null) { ASSERT_EQ(NULL, null); }  //NOLINT
  CharacterRange(uc16 from, uc16 to) : from_(from), to_(to) { }
  static void AddClassEscape(uc16 type, ZoneList<CharacterRange>* ranges);
  static Vector<const uc16> GetWordBounds();
  static inline CharacterRange Singleton(uc16 value) {
    return CharacterRange(value, value);
  }
  static inline CharacterRange Range(uc16 from, uc16 to) {
    ASSERT(from <= to);
    return CharacterRange(from, to);
  }
  static inline CharacterRange Everything() {
    return CharacterRange(0, 0xFFFF);
  }
  bool Contains(uc16 i) { return from_ <= i && i <= to_; }
  uc16 from() const { return from_; }
  void set_from(uc16 value) { from_ = value; }
  uc16 to() const { return to_; }
  void set_to(uc16 value) { to_ = value; }
  bool is_valid() { return from_ <= to_; }
  bool IsEverything(uc16 max) { return from_ == 0 && to_ >= max; }
  bool IsSingleton() { return (from_ == to_); }
  void AddCaseEquivalents(ZoneList<CharacterRange>* ranges);
  static void Split(ZoneList<CharacterRange>* base,
                    Vector<const uc16> overlay,
                    ZoneList<CharacterRange>** included,
                    ZoneList<CharacterRange>** excluded);

  static const int kRangeCanonicalizeMax = 0x346;
  static const int kStartMarker = (1 << 24);
  static const int kPayloadMask = (1 << 24) - 1;

 private:
  uc16 from_;
  uc16 to_;
};


template <typename Node, class Callback>
static void DoForEach(Node* node, Callback* callback);


// A zone splay tree.  The config type parameter encapsulates the
// different configurations of a concrete splay tree:
//
//   typedef Key: the key type
//   typedef Value: the value type
//   static const kNoKey: the dummy key used when no key is set
//   static const kNoValue: the dummy value used to initialize nodes
//   int (Compare)(Key& a, Key& b) -> {-1, 0, 1}: comparison function
//
template <typename Config>
class ZoneSplayTree : public ZoneObject {
 public:
  typedef typename Config::Key Key;
  typedef typename Config::Value Value;

  class Locator;

  ZoneSplayTree() : root_(NULL) { }

  // Inserts the given key in this tree with the given value.  Returns
  // true if a node was inserted, otherwise false.  If found the locator
  // is enabled and provides access to the mapping for the key.
  bool Insert(const Key& key, Locator* locator);

  // Looks up the key in this tree and returns true if it was found,
  // otherwise false.  If the node is found the locator is enabled and
  // provides access to the mapping for the key.
  bool Find(const Key& key, Locator* locator);

  // Finds the mapping with the greatest key less than or equal to the
  // given key.
  bool FindGreatestLessThan(const Key& key, Locator* locator);

  // Find the mapping with the greatest key in this tree.
  bool FindGreatest(Locator* locator);

  // Finds the mapping with the least key greater than or equal to the
  // given key.
  bool FindLeastGreaterThan(const Key& key, Locator* locator);

  // Find the mapping with the least key in this tree.
  bool FindLeast(Locator* locator);

  // Remove the node with the given key from the tree.
  bool Remove(const Key& key);

  bool is_empty() { return root_ == NULL; }

  // Perform the splay operation for the given key. Moves the node with
  // the given key to the top of the tree.  If no node has the given
  // key, the last node on the search path is moved to the top of the
  // tree.
  void Splay(const Key& key);

  class Node : public ZoneObject {
   public:
    Node(const Key& key, const Value& value)
        : key_(key),
          value_(value),
          left_(NULL),
          right_(NULL) { }
    Key key() { return key_; }
    Value value() { return value_; }
    Node* left() { return left_; }
    Node* right() { return right_; }
   private:
    friend class ZoneSplayTree;
    friend class Locator;
    Key key_;
    Value value_;
    Node* left_;
    Node* right_;
  };

  // A locator provides access to a node in the tree without actually
  // exposing the node.
  class Locator {
   public:
    explicit Locator(Node* node) : node_(node) { }
    Locator() : node_(NULL) { }
    const Key& key() { return node_->key_; }
    Value& value() { return node_->value_; }
    void set_value(const Value& value) { node_->value_ = value; }
    inline void bind(Node* node) { node_ = node; }
   private:
    Node* node_;
  };

  template <class Callback>
  void ForEach(Callback* c) {
    DoForEach<typename ZoneSplayTree<Config>::Node, Callback>(root_, c);
  }

 private:
  Node* root_;
};


// A set of unsigned integers that behaves especially well on small
// integers (< 32).  May do zone-allocation.
class OutSet: public ZoneObject {
 public:
  OutSet() : first_(0), remaining_(NULL), successors_(NULL) { }
  OutSet* Extend(unsigned value);
  bool Get(unsigned value);
  static const unsigned kFirstLimit = 32;

 private:
  // Destructively set a value in this set.  In most cases you want
  // to use Extend instead to ensure that only one instance exists
  // that contains the same values.
  void Set(unsigned value);

  // The successors are a list of sets that contain the same values
  // as this set and the one more value that is not present in this
  // set.
  ZoneList<OutSet*>* successors() { return successors_; }

  OutSet(uint32_t first, ZoneList<unsigned>* remaining)
      : first_(first), remaining_(remaining), successors_(NULL) { }
  uint32_t first_;
  ZoneList<unsigned>* remaining_;
  ZoneList<OutSet*>* successors_;
  friend class GenerationVariant;
};


// A mapping from integers, specified as ranges, to a set of integers.
// Used for mapping character ranges to choices.
class DispatchTable : public ZoneObject {
 public:
  class Entry {
   public:
    Entry() : from_(0), to_(0), out_set_(NULL) { }
    Entry(uc16 from, uc16 to, OutSet* out_set)
        : from_(from), to_(to), out_set_(out_set) { }
    uc16 from() { return from_; }
    uc16 to() { return to_; }
    void set_to(uc16 value) { to_ = value; }
    void AddValue(int value) { out_set_ = out_set_->Extend(value); }
    OutSet* out_set() { return out_set_; }
   private:
    uc16 from_;
    uc16 to_;
    OutSet* out_set_;
  };

  class Config {
   public:
    typedef uc16 Key;
    typedef Entry Value;
    static const uc16 kNoKey;
    static const Entry kNoValue;
    static inline int Compare(uc16 a, uc16 b) {
      if (a == b)
        return 0;
      else if (a < b)
        return -1;
      else
        return 1;
    }
  };

  void AddRange(CharacterRange range, int value);
  OutSet* Get(uc16 value);
  void Dump();

  template <typename Callback>
  void ForEach(Callback* callback) { return tree()->ForEach(callback); }
 private:
  // There can't be a static empty set since it allocates its
  // successors in a zone and caches them.
  OutSet* empty() { return &empty_; }
  OutSet empty_;
  ZoneSplayTree<Config>* tree() { return &tree_; }
  ZoneSplayTree<Config> tree_;
};


#define FOR_EACH_NODE_TYPE(VISIT)                                    \
  VISIT(End)                                                         \
  VISIT(Action)                                                      \
  VISIT(Choice)                                                      \
  VISIT(BackReference)                                               \
  VISIT(Text)


#define FOR_EACH_REG_EXP_TREE_TYPE(VISIT)                            \
  VISIT(Disjunction)                                                 \
  VISIT(Alternative)                                                 \
  VISIT(Assertion)                                                   \
  VISIT(CharacterClass)                                              \
  VISIT(Atom)                                                        \
  VISIT(Quantifier)                                                  \
  VISIT(Capture)                                                     \
  VISIT(Lookahead)                                                   \
  VISIT(BackReference)                                               \
  VISIT(Empty)                                                       \
  VISIT(Text)


#define FORWARD_DECLARE(Name) class RegExp##Name;
FOR_EACH_REG_EXP_TREE_TYPE(FORWARD_DECLARE)
#undef FORWARD_DECLARE


class TextElement {
 public:
  enum Type {UNINITIALIZED, ATOM, CHAR_CLASS};
  TextElement() : type(UNINITIALIZED) { }
  explicit TextElement(Type t) : type(t), cp_offset(-1) { }
  static TextElement Atom(RegExpAtom* atom);
  static TextElement CharClass(RegExpCharacterClass* char_class);
  int length();
  Type type;
  union {
    RegExpAtom* u_atom;
    RegExpCharacterClass* u_char_class;
  } data;
  int cp_offset;
};


class GenerationVariant;


struct NodeInfo {
  enum TriBool {
    UNKNOWN = -1, FALSE = 0, TRUE = 1
  };

  NodeInfo()
      : being_analyzed(false),
        been_analyzed(false),
        follows_word_interest(false),
        follows_newline_interest(false),
        follows_start_interest(false),
        at_end(false),
        visited(false) { }

  // Returns true if the interests and assumptions of this node
  // matches the given one.
  bool Matches(NodeInfo* that) {
    return (at_end == that->at_end) &&
           (follows_word_interest == that->follows_word_interest) &&
           (follows_newline_interest == that->follows_newline_interest) &&
           (follows_start_interest == that->follows_start_interest);
  }

  // Updates the interests of this node given the interests of the
  // node preceding it.
  void AddFromPreceding(NodeInfo* that) {
    at_end |= that->at_end;
    follows_word_interest |= that->follows_word_interest;
    follows_newline_interest |= that->follows_newline_interest;
    follows_start_interest |= that->follows_start_interest;
  }

  bool HasLookbehind() {
    return follows_word_interest ||
           follows_newline_interest ||
           follows_start_interest;
  }

  // Sets the interests of this node to include the interests of the
  // following node.
  void AddFromFollowing(NodeInfo* that) {
    follows_word_interest |= that->follows_word_interest;
    follows_newline_interest |= that->follows_newline_interest;
    follows_start_interest |= that->follows_start_interest;
  }

  void ResetCompilationState() {
    being_analyzed = false;
    been_analyzed = false;
  }

  bool being_analyzed: 1;
  bool been_analyzed: 1;

  // These bits are set of this node has to know what the preceding
  // character was.
  bool follows_word_interest: 1;
  bool follows_newline_interest: 1;
  bool follows_start_interest: 1;

  bool at_end: 1;
  bool visited: 1;
};


class SiblingList {
 public:
  SiblingList() : list_(NULL) { }
  int length() {
    return list_ == NULL ? 0 : list_->length();
  }
  void Ensure(RegExpNode* parent) {
    if (list_ == NULL) {
      list_ = new ZoneList<RegExpNode*>(2);
      list_->Add(parent);
    }
  }
  void Add(RegExpNode* node) { list_->Add(node); }
  RegExpNode* Get(int index) { return list_->at(index); }
 private:
  ZoneList<RegExpNode*>* list_;
};


// Details of a quick mask-compare check that can look ahead in the
// input stream.
class QuickCheckDetails {
 public:
  QuickCheckDetails()
      : characters_(0),
        mask_(0),
        value_(0) { }
  explicit QuickCheckDetails(int characters)
      : characters_(characters),
        mask_(0),
        value_(0) { }
  bool Rationalize(bool ascii);
  // Merge in the information from another branch of an alternation.
  void Merge(QuickCheckDetails* other, int from_index);
  // Advance the current position by some amount.
  void Advance(int by, bool ascii);
  void Clear();
  struct Position {
    Position() : mask(0), value(0), determines_perfectly(false) { }
    uc16 mask;
    uc16 value;
    bool determines_perfectly;
  };
  int characters() { return characters_; }
  void set_characters(int characters) { characters_ = characters; }
  Position* positions(int index) {
    ASSERT(index >= 0);
    ASSERT(index < characters_);
    return positions_ + index;
  }
  uint32_t mask() { return mask_; }
  uint32_t value() { return value_; }

 private:
  // How many characters do we have quick check information from.  This is
  // the same for all branches of a choice node.
  int characters_;
  Position positions_[4];
  // These values are the condensate of the above array after Rationalize().
  uint32_t mask_;
  uint32_t value_;
};


class RegExpNode: public ZoneObject {
 public:
  RegExpNode() : variants_generated_(0) { }
  virtual ~RegExpNode();
  virtual void Accept(NodeVisitor* visitor) = 0;
  // Generates a goto to this node or actually generates the code at this point.
  // Until the implementation is complete we will return true for success and
  // false for failure.
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant) = 0;
  // How many characters must this node consume at a minimum in order to
  // succeed.
  virtual int EatsAtLeast(int recursion_depth) = 0;
  // Emits some quick code that checks whether the preloaded characters match.
  // Falls through on certain failure, jumps to the label on possible success.
  // If the node cannot make a quick check it does nothing and returns false.
  bool EmitQuickCheck(RegExpCompiler* compiler,
                      GenerationVariant* variant,
                      bool preload_has_checked_bounds,
                      Label* on_possible_success,
                      QuickCheckDetails* details_return,
                      bool fall_through_on_failure);
  // For a given number of characters this returns a mask and a value.  The
  // next n characters are anded with the mask and compared with the value.
  // A comparison failure indicates the node cannot match the next n characters.
  // A comparison success indicates the node may match.
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in) = 0;
  static const int kNodeIsTooComplexForGreedyLoops = -1;
  virtual int GreedyLoopTextLength() { return kNodeIsTooComplexForGreedyLoops; }
  Label* label() { return &label_; }
  static const int kMaxVariantsGenerated = 10;

  // Propagates the given interest information forward.  When seeing
  // \bfoo for instance, the \b is implemented by propagating forward
  // to the 'foo' string that it should only succeed if its first
  // character is a letter xor the previous character was a letter.
  virtual RegExpNode* PropagateForward(NodeInfo* info) = 0;

  NodeInfo* info() { return &info_; }

  void AddSibling(RegExpNode* node) { siblings_.Add(node); }

  // Static version of EnsureSibling that expresses the fact that the
  // result has the same type as the input.
  template <class C>
  static C* EnsureSibling(C* node, NodeInfo* info, bool* cloned) {
    return static_cast<C*>(node->EnsureSibling(info, cloned));
  }

  SiblingList* siblings() { return &siblings_; }
  void set_siblings(SiblingList* other) { siblings_ = *other; }

 protected:
  enum LimitResult { DONE, FAIL, CONTINUE };
  LimitResult LimitVersions(RegExpCompiler* compiler,
                            GenerationVariant* variant);

  // Returns a sibling of this node whose interests and assumptions
  // match the ones in the given node info.  If no sibling exists NULL
  // is returned.
  RegExpNode* TryGetSibling(NodeInfo* info);

  // Returns a sibling of this node whose interests match the ones in
  // the given node info.  The info must not contain any assertions.
  // If no node exists a new one will be created by cloning the current
  // node.  The result will always be an instance of the same concrete
  // class as this node.
  RegExpNode* EnsureSibling(NodeInfo* info, bool* cloned);

  // Returns a clone of this node initialized using the copy constructor
  // of its concrete class.  Note that the node may have to be pre-
  // processed before it is on a useable state.
  virtual RegExpNode* Clone() = 0;

 private:
  Label label_;
  NodeInfo info_;
  SiblingList siblings_;
  int variants_generated_;
};


// A simple closed interval.
class Interval {
 public:
  Interval() : from_(kNone), to_(kNone) { }
  Interval(int from, int to) : from_(from), to_(to) { }
  Interval Union(Interval that) {
    if (that.from_ == kNone)
      return *this;
    else if (from_ == kNone)
      return that;
    else
      return Interval(Min(from_, that.from_), Max(to_, that.to_));
  }
  bool Contains(int value) {
    return (from_ <= value) && (value <= to_);
  }
  bool is_empty() { return from_ == kNone; }
  int from() { return from_; }
  int to() { return to_; }
  static Interval Empty() { return Interval(); }
  static const int kNone = -1;
 private:
  int from_;
  int to_;
};


class SeqRegExpNode: public RegExpNode {
 public:
  explicit SeqRegExpNode(RegExpNode* on_success)
      : on_success_(on_success) { }
  RegExpNode* on_success() { return on_success_; }
  void set_on_success(RegExpNode* node) { on_success_ = node; }
 private:
  RegExpNode* on_success_;
};


class ActionNode: public SeqRegExpNode {
 public:
  enum Type {
    SET_REGISTER,
    INCREMENT_REGISTER,
    STORE_POSITION,
    BEGIN_SUBMATCH,
    POSITIVE_SUBMATCH_SUCCESS,
    EMPTY_MATCH_CHECK,
    CLEAR_CAPTURES
  };
  static ActionNode* SetRegister(int reg, int val, RegExpNode* on_success);
  static ActionNode* IncrementRegister(int reg, RegExpNode* on_success);
  static ActionNode* StorePosition(int reg, RegExpNode* on_success);
  static ActionNode* ClearCaptures(Interval range, RegExpNode* on_success);
  static ActionNode* BeginSubmatch(int stack_pointer_reg,
                                   int position_reg,
                                   RegExpNode* on_success);
  static ActionNode* PositiveSubmatchSuccess(int stack_pointer_reg,
                                             int restore_reg,
                                             RegExpNode* on_success);
  static ActionNode* EmptyMatchCheck(int start_register,
                                     int repetition_register,
                                     int repetition_limit,
                                     RegExpNode* on_success);
  virtual void Accept(NodeVisitor* visitor);
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);
  virtual int EatsAtLeast(int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int filled_in) {
    return on_success()->GetQuickCheckDetails(details, compiler, filled_in);
  }
  virtual RegExpNode* PropagateForward(NodeInfo* info);
  Type type() { return type_; }
  // TODO(erikcorry): We should allow some action nodes in greedy loops.
  virtual int GreedyLoopTextLength() { return kNodeIsTooComplexForGreedyLoops; }
  virtual ActionNode* Clone() { return new ActionNode(*this); }

 private:
  union {
    struct {
      int reg;
      int value;
    } u_store_register;
    struct {
      int reg;
    } u_increment_register;
    struct {
      int reg;
    } u_position_register;
    struct {
      int stack_pointer_register;
      int current_position_register;
    } u_submatch;
    struct {
      int start_register;
      int repetition_register;
      int repetition_limit;
    } u_empty_match_check;
    struct {
      int range_from;
      int range_to;
    } u_clear_captures;
  } data_;
  ActionNode(Type type, RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        type_(type) { }
  Type type_;
  friend class DotPrinter;
};


class TextNode: public SeqRegExpNode {
 public:
  TextNode(ZoneList<TextElement>* elms,
           RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        elms_(elms) { }
  TextNode(RegExpCharacterClass* that,
           RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        elms_(new ZoneList<TextElement>(1)) {
    elms_->Add(TextElement::CharClass(that));
  }
  virtual void Accept(NodeVisitor* visitor);
  virtual RegExpNode* PropagateForward(NodeInfo* info);
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);
  virtual int EatsAtLeast(int recursion_depth);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in);
  ZoneList<TextElement>* elements() { return elms_; }
  void MakeCaseIndependent();
  virtual int GreedyLoopTextLength();
  virtual TextNode* Clone() {
    TextNode* result = new TextNode(*this);
    result->CalculateOffsets();
    return result;
  }
  void CalculateOffsets();

 private:
  enum TextEmitPassType {
    NON_ASCII_MATCH,
    CHARACTER_MATCH,
    CASE_CHARACTER_MATCH,
    CHARACTER_CLASS_MATCH
  };
  void TextEmitPass(RegExpCompiler* compiler,
                    TextEmitPassType pass,
                    bool preloaded,
                    GenerationVariant* variant,
                    bool first_element_checked,
                    int* checked_up_to);
  int Length();
  ZoneList<TextElement>* elms_;
};


class BackReferenceNode: public SeqRegExpNode {
 public:
  BackReferenceNode(int start_reg,
                    int end_reg,
                    RegExpNode* on_success)
      : SeqRegExpNode(on_success),
        start_reg_(start_reg),
        end_reg_(end_reg) { }
  virtual void Accept(NodeVisitor* visitor);
  int start_register() { return start_reg_; }
  int end_register() { return end_reg_; }
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);
  virtual int EatsAtLeast(int recursion_depth) { return 0; }
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in) {
    return;
  }
  virtual RegExpNode* PropagateForward(NodeInfo* info);
  virtual BackReferenceNode* Clone() { return new BackReferenceNode(*this); }

 private:
  int start_reg_;
  int end_reg_;
};


class EndNode: public RegExpNode {
 public:
  enum Action { ACCEPT, BACKTRACK, NEGATIVE_SUBMATCH_SUCCESS };
  explicit EndNode(Action action) : action_(action) { }
  virtual void Accept(NodeVisitor* visitor);
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);
  virtual int EatsAtLeast(int recursion_depth) { return 0; }
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in) {
    // Returning 0 from EatsAtLeast should ensure we never get here.
    UNREACHABLE();
  }
  virtual RegExpNode* PropagateForward(NodeInfo* info);
  virtual EndNode* Clone() { return new EndNode(*this); }

 protected:
  void EmitInfoChecks(RegExpMacroAssembler* macro, GenerationVariant* variant);

 private:
  Action action_;
};


class NegativeSubmatchSuccess: public EndNode {
 public:
  NegativeSubmatchSuccess(int stack_pointer_reg, int position_reg)
      : EndNode(NEGATIVE_SUBMATCH_SUCCESS),
        stack_pointer_register_(stack_pointer_reg),
        current_position_register_(position_reg) { }
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);

 private:
  int stack_pointer_register_;
  int current_position_register_;
};


class Guard: public ZoneObject {
 public:
  enum Relation { LT, GEQ };
  Guard(int reg, Relation op, int value)
      : reg_(reg),
        op_(op),
        value_(value) { }
  int reg() { return reg_; }
  Relation op() { return op_; }
  int value() { return value_; }

 private:
  int reg_;
  Relation op_;
  int value_;
};


class GuardedAlternative {
 public:
  explicit GuardedAlternative(RegExpNode* node) : node_(node), guards_(NULL) { }
  void AddGuard(Guard* guard);
  RegExpNode* node() { return node_; }
  void set_node(RegExpNode* node) { node_ = node; }
  ZoneList<Guard*>* guards() { return guards_; }

 private:
  RegExpNode* node_;
  ZoneList<Guard*>* guards_;
};


class AlternativeGeneration;


class ChoiceNode: public RegExpNode {
 public:
  explicit ChoiceNode(int expected_size)
      : alternatives_(new ZoneList<GuardedAlternative>(expected_size)),
        table_(NULL),
        being_calculated_(false) { }
  virtual void Accept(NodeVisitor* visitor);
  void AddAlternative(GuardedAlternative node) { alternatives()->Add(node); }
  ZoneList<GuardedAlternative>* alternatives() { return alternatives_; }
  DispatchTable* GetTable(bool ignore_case);
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);
  virtual int EatsAtLeast(int recursion_depth);
  int EatsAtLeastHelper(int recursion_depth, RegExpNode* ignore_this_node);
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in);
  virtual RegExpNode* PropagateForward(NodeInfo* info);
  virtual ChoiceNode* Clone() { return new ChoiceNode(*this); }

  bool being_calculated() { return being_calculated_; }
  void set_being_calculated(bool b) { being_calculated_ = b; }

 protected:
  int GreedyLoopTextLength(GuardedAlternative *alternative);
  ZoneList<GuardedAlternative>* alternatives_;

 private:
  friend class DispatchTableConstructor;
  friend class Analysis;
  void GenerateGuard(RegExpMacroAssembler* macro_assembler,
                     Guard *guard,
                     GenerationVariant* variant);
  int CalculatePreloadCharacters(RegExpCompiler* compiler);
  bool EmitOutOfLineContinuation(RegExpCompiler* compiler,
                                 GenerationVariant* variant,
                                 GuardedAlternative alternative,
                                 AlternativeGeneration* alt_gen,
                                 int preload_characters,
                                 bool next_expects_preload);
  DispatchTable* table_;
  bool being_calculated_;
};


class LoopChoiceNode: public ChoiceNode {
 public:
  explicit LoopChoiceNode(bool body_can_be_zero_length)
      : ChoiceNode(2),
        loop_node_(NULL),
        continue_node_(NULL),
        body_can_be_zero_length_(body_can_be_zero_length) { }
  void AddLoopAlternative(GuardedAlternative alt);
  void AddContinueAlternative(GuardedAlternative alt);
  virtual bool Emit(RegExpCompiler* compiler, GenerationVariant* variant);
  virtual int EatsAtLeast(int recursion_depth);  // Returns 0.
  virtual void GetQuickCheckDetails(QuickCheckDetails* details,
                                    RegExpCompiler* compiler,
                                    int characters_filled_in);
  virtual LoopChoiceNode* Clone() { return new LoopChoiceNode(*this); }
  RegExpNode* loop_node() { return loop_node_; }
  RegExpNode* continue_node() { return continue_node_; }
  bool body_can_be_zero_length() { return body_can_be_zero_length_; }
  virtual void Accept(NodeVisitor* visitor);

 private:
  // AddAlternative is made private for loop nodes because alternatives
  // should not be added freely, we need to keep track of which node
  // goes back to the node itself.
  void AddAlternative(GuardedAlternative node) {
    ChoiceNode::AddAlternative(node);
  }

  RegExpNode* loop_node_;
  RegExpNode* continue_node_;
  bool body_can_be_zero_length_;
};


// There are many ways to generate code for a node.  This class encapsulates
// the current way we should be generating.  In other words it encapsulates
// the current state of the code generator.
class GenerationVariant {
 public:
  class DeferredAction {
   public:
    DeferredAction(ActionNode::Type type, int reg)
        : type_(type), reg_(reg), next_(NULL) { }
    DeferredAction* next() { return next_; }
    bool Mentions(int reg);
    int reg() { return reg_; }
    ActionNode::Type type() { return type_; }
   private:
    ActionNode::Type type_;
    int reg_;
    DeferredAction* next_;
    friend class GenerationVariant;
  };

  class DeferredCapture: public DeferredAction {
   public:
    DeferredCapture(int reg, GenerationVariant* variant)
        : DeferredAction(ActionNode::STORE_POSITION, reg),
          cp_offset_(variant->cp_offset()) { }
    int cp_offset() { return cp_offset_; }
   private:
    int cp_offset_;
    void set_cp_offset(int cp_offset) { cp_offset_ = cp_offset; }
  };

  class DeferredSetRegister :public DeferredAction {
   public:
    DeferredSetRegister(int reg, int value)
        : DeferredAction(ActionNode::SET_REGISTER, reg),
          value_(value) { }
    int value() { return value_; }
   private:
    int value_;
  };

  class DeferredClearCaptures : public DeferredAction {
   public:
    explicit DeferredClearCaptures(Interval range)
        : DeferredAction(ActionNode::CLEAR_CAPTURES, -1),
          range_(range) { }
    Interval range() { return range_; }
   private:
    Interval range_;
  };

  class DeferredIncrementRegister: public DeferredAction {
   public:
    explicit DeferredIncrementRegister(int reg)
        : DeferredAction(ActionNode::INCREMENT_REGISTER, reg) { }
  };

  GenerationVariant()
      : cp_offset_(0),
        actions_(NULL),
        backtrack_(NULL),
        stop_node_(NULL),
        loop_label_(NULL),
        characters_preloaded_(0),
        bound_checked_up_to_(0) { }
  bool Flush(RegExpCompiler* compiler, RegExpNode* successor);
  int cp_offset() { return cp_offset_; }
  DeferredAction* actions() { return actions_; }
  bool is_trivial() {
    return backtrack_ == NULL &&
           actions_ == NULL &&
           cp_offset_ == 0 &&
           characters_preloaded_ == 0 &&
           bound_checked_up_to_ == 0 &&
           quick_check_performed_.characters() == 0;
  }
  Label* backtrack() { return backtrack_; }
  Label* loop_label() { return loop_label_; }
  RegExpNode* stop_node() { return stop_node_; }
  int characters_preloaded() { return characters_preloaded_; }
  int bound_checked_up_to() { return bound_checked_up_to_; }
  QuickCheckDetails* quick_check_performed() { return &quick_check_performed_; }
  bool mentions_reg(int reg);
  // Returns true if a deferred position store exists to the specified
  // register and stores the offset in the out-parameter.  Otherwise
  // returns false.
  bool GetStoredPosition(int reg, int* cp_offset);
  // These set methods and AdvanceVariant should be used only on new
  // GenerationVariants - the intention is that GenerationVariants are
  // immutable after creation.
  void add_action(DeferredAction* new_action) {
    ASSERT(new_action->next_ == NULL);
    new_action->next_ = actions_;
    actions_ = new_action;
  }
  void set_backtrack(Label* backtrack) { backtrack_ = backtrack; }
  void set_stop_node(RegExpNode* node) { stop_node_ = node; }
  void set_loop_label(Label* label) { loop_label_ = label; }
  void set_characters_preloaded(int cpre) { characters_preloaded_ = cpre; }
  void set_bound_checked_up_to(int to) { bound_checked_up_to_ = to; }
  void set_quick_check_performed(QuickCheckDetails* d) {
    quick_check_performed_ = *d;
  }
  void clear_quick_check_performed() {
  }
  void AdvanceVariant(int by, bool ascii);
 private:
  int FindAffectedRegisters(OutSet* affected_registers);
  void PerformDeferredActions(RegExpMacroAssembler* macro,
                               int max_register,
                               OutSet& affected_registers);
  void RestoreAffectedRegisters(RegExpMacroAssembler* macro,
                                int max_register,
                                OutSet& affected_registers);
  void PushAffectedRegisters(RegExpMacroAssembler* macro,
                             int max_register,
                             OutSet& affected_registers);
  int cp_offset_;
  DeferredAction* actions_;
  Label* backtrack_;
  RegExpNode* stop_node_;
  Label* loop_label_;
  int characters_preloaded_;
  int bound_checked_up_to_;
  QuickCheckDetails quick_check_performed_;
};


class NodeVisitor {
 public:
  virtual ~NodeVisitor() { }
#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that) = 0;
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  virtual void VisitLoopChoice(LoopChoiceNode* that) { VisitChoice(that); }
};


// Node visitor used to add the start set of the alternatives to the
// dispatch table of a choice node.
class DispatchTableConstructor: public NodeVisitor {
 public:
  DispatchTableConstructor(DispatchTable* table, bool ignore_case)
      : table_(table),
        choice_index_(-1),
        ignore_case_(ignore_case) { }

  void BuildTable(ChoiceNode* node);

  void AddRange(CharacterRange range) {
    table()->AddRange(range, choice_index_);
  }

  void AddInverse(ZoneList<CharacterRange>* ranges);

#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT

  DispatchTable* table() { return table_; }
  void set_choice_index(int value) { choice_index_ = value; }

 protected:
  DispatchTable *table_;
  int choice_index_;
  bool ignore_case_;
};


// Assertion propagation moves information about assertions such as
// \b to the affected nodes.  For instance, in /.\b./ information must
// be propagated to the first '.' that whatever follows needs to know
// if it matched a word or a non-word, and to the second '.' that it
// has to check if it succeeds a word or non-word.  In this case the
// result will be something like:
//
//   +-------+        +------------+
//   |   .   |        |      .     |
//   +-------+  --->  +------------+
//   | word? |        | check word |
//   +-------+        +------------+
class Analysis: public NodeVisitor {
 public:
  explicit Analysis(bool ignore_case)
      : ignore_case_(ignore_case) { }
  void EnsureAnalyzed(RegExpNode* node);

#define DECLARE_VISIT(Type)                                          \
  virtual void Visit##Type(Type##Node* that);
FOR_EACH_NODE_TYPE(DECLARE_VISIT)
#undef DECLARE_VISIT
  virtual void VisitLoopChoice(LoopChoiceNode* that);

 private:
  bool ignore_case_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(Analysis);
};


struct RegExpCompileData {
  RegExpCompileData()
    : tree(NULL),
      node(NULL),
      simple(true),
      capture_count(0) { }
  RegExpTree* tree;
  RegExpNode* node;
  bool simple;
  Handle<String> error;
  int capture_count;
};


class RegExpEngine: public AllStatic {
 public:
  static Handle<FixedArray> Compile(RegExpCompileData* input,
                                    bool ignore_case,
                                    bool multiline,
                                    Handle<String> pattern,
                                    bool is_ascii);

  static void DotPrint(const char* label, RegExpNode* node, bool ignore_case);
};


} }  // namespace v8::internal

#endif  // V8_JSREGEXP_H_
