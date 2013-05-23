#ifndef VAST_EXPRESSION_H
#define VAST_EXPRESSION_H

#include "vast/util/visitor.h"
#include "vast/event.h"
#include "vast/operator.h"
#include "vast/schema.h"

namespace vast {

// Forward declarations
class expression;
std::string to_string(expression const&);

namespace expr {

// Forward declarations
class node;
class timestamp_extractor;
class name_extractor;
class id_extractor;
class offset_extractor;
class type_extractor;
class conjunction;
class disjunction;
class relation;
class constant;

typedef util::const_visitor<
    node
 ,  timestamp_extractor
 ,  name_extractor
 ,  id_extractor
 ,  offset_extractor
 ,  type_extractor
 ,  conjunction
 ,  disjunction
 ,  relation
 ,  constant
> const_visitor;

typedef util::visitor<
    node
 ,  timestamp_extractor
 ,  name_extractor
 ,  id_extractor
 ,  offset_extractor
 ,  type_extractor
 ,  conjunction
 ,  disjunction
 ,  relation
 ,  constant
> visitor;

/// The base class for nodes in the expression tree.
class node
{
  node(node const&) = delete;

public:
  virtual ~node() = default;

  /// Gets the result of the sub-tree induced by this node.
  /// @return The value of this node.
  value const& result() const;

  /// Determines whether the result is available without evaluation.
  ///
  /// @return `true` if the result can be obtained without a call to
  /// node::eval.
  bool ready() const;

  /// Resets the sub-tree induced by this node.
  virtual void reset();

  /// Evaluates the sub-tree induced by this node.
  virtual void eval() = 0;

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

protected:
  node() = default;

  value result_ = invalid;
  bool ready_ = false;
};

/// The base class for extractor nodes.
class extractor : public node
{
public:
  virtual void feed(event const* event);
  //event const* event() const;

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

protected:
  virtual void eval() = 0;

  event const* event_;
};

/// Extracts the event timestamp.
class timestamp_extractor : public extractor
{
public:
  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();
};

/// Extracts the event name.
class name_extractor : public extractor
{
public:
  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();
};

/// Extracts the event ID.
class id_extractor : public extractor
{
public:
  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();
};

/// Extracts an argument at a given offset.
class offset_extractor : public extractor
{
public:
  offset_extractor(std::vector<size_t> offsets);

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

  std::vector<size_t> const& offsets() const;

private:
  virtual void eval();
  std::vector<size_t> offsets_;
};

class type_extractor : public extractor
{
public:
  type_extractor(value_type type);

  virtual void feed(event const* e);
  virtual void reset();

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

  value_type type() const;

private:
  virtual void eval();

  value_type type_;
  std::vector<std::pair<record const*, size_t>> pos_;
};

/// An n-ary operator.
class n_ary_operator : public node
{
public:
  void add(std::unique_ptr<node> operand);
  virtual void reset();

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

  std::vector<std::unique_ptr<node>>& operands();
  std::vector<std::unique_ptr<node>> const& operands() const;

protected:
  virtual void eval() = 0;
  std::vector<std::unique_ptr<node>> operands_;
};

/// A conjunction.
class conjunction : public n_ary_operator
{
public:
  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();
};

/// A disjunction.
class disjunction : public n_ary_operator
{
public:
  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();
};

/// A relational operator.
class relation : public n_ary_operator
{
public:
  typedef std::function<bool(value const&, value const&)>
    binary_predicate;

  relation(relational_operator op);

  bool test(value const& lhs, value const& rhs) const;
  relational_operator type() const;

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();

  binary_predicate op_;
  relational_operator op_type_;
};

/// A constant value.
class constant : public node
{
public:
  constant(value val);
  virtual void reset();

  VAST_ACCEPT_CONST(const_visitor)
  VAST_ACCEPT(visitor)

private:
  virtual void eval();
};

} // namespace expr

/// A query expression.
class expression
{
public:
  /// Constructs an empty expression.
  expression() = default;

  /// Copy-constructs an expression
  /// @param other The expression to copy.
  expression(expression const& other);

  /// Move-constructs an expression.
  /// @param other The expression to move.
  expression(expression&& other);

  /// Assigns an expression.
  /// @param other The RHS of the assignment.
  expression& operator=(expression other);

  /// Parses a given expression.
  /// @param str The query expression to transform into an AST.
  /// @param sch The schema to use to resolve event clauses.
  void parse(std::string str, schema sch = {});

  /// Evaluates an event with respect to the root node.
  /// @param e The event to evaluate against the expression.
  /// @return `true` if @a event matches the expression.
  bool eval(event const& e);

  /// Allow a visitor to process the expression.
  /// @param v The visitor
  void accept(expr::const_visitor& v) const;

  /// Allow a visitor to process the expression.
  /// @param v The visitor
  void accept(expr::visitor& v);

private:
  friend io::access;
  void serialize(io::serializer& sink);
  void deserialize(io::deserializer& source);

  friend bool operator==(expression const& x, expression const& y);
  friend bool operator!=(expression const& x, expression const& y);

  std::string str_;
  schema schema_;
  std::unique_ptr<expr::node> root_;
  std::vector<expr::extractor*> extractors_;
};

} // namespace vast

#endif
