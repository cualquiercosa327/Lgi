bool identifier(ParseContext &c)
{
	char16 *t = c.t();
	if (*t == '_' OR isalpha(*t))
	{
		c.Next();
		return true;
	}
	return false;
}

bool class_name(ParseContext &c)
{
	return identifier(c);
}

bool enum_name(ParseContext &c)
{
	return identifier(c);
}

bool typedef_name(ParseContext &c)
{
	return identifier(c);
}

bool is(ParseContext &c, char *v)
{
	char16 *t = c.t();
	while (*t AND *v)
	{
		if (*t != *v) return false;
	}
	
	if (NOT *t AND NOT *v)
	{
		c.Next();
		return true;
	}
	
	return false;	
}

extern bool assignment_expression(ParseContext &c);
extern bool logical_AND_expression(ParseContext &c);
extern bool inclusive_OR_expression(ParseContext &c);
extern bool exclusive_OR_expression(ParseContext &c);

#define Test(cond) \
	{ int i = c.Cur; if (cond) { return true; } c.Cur = i; }

bool expression(ParseContext &c)
{
	Test(assignment_expression(c));
	Test(expression(c) AND is(c, ",") AND assignment_expression(c));
	return false;
}

bool logical_OR_expression(ParseContext &c)
{
	Test(logical_AND_expression(c));
	Test(logical_OR_expression(c) AND is(c, "||") AND logical_AND_expression(c));
	return false;
}

bool logical_AND_expression(ParseContext &c)
{
	Test(inclusive_OR_expression(c));
	Test(logical_AND_expression(c) AND is(c, "&&") AND inclusive_OR_expression(c));
	return false;
}

bool inclusive_OR_expression(ParseContext &c)
{
	Test(exclusive_OR_expression(c));
	Test(inclusive_OR_expression(c) AND is(c, "|") AND exclusive_OR_expression(c));
	return false;
}

bool unary_operator(ParseContext &c)
{
	if
	(
		is(c, "*")
		OR
		is(c, "&")
		OR
		is(c, "+")
		OR
		is(c, "-")
		OR
		is(c, "!")
		OR
		is(c, "~")
	)
	{
		c.Next();
		return true;
	}
	
	return false;
}

primary-expression
    literal
    this
    :: identifier
    :: operator-function-name
    :: qualified-name
    ( expression )
    name

bool postfix_expression(ParseContext &c)
{
	Test(primary_expression(c));
	Test(postfix_expression(c) AND is(c, "[") AND expression(c) AND is(c, "]"));
	Test(postfix_expression(c) AND is(c, "(") AND expression_list(c) AND is(c, ")"));
	Test(postfix_expression(c) AND is(c, "(") AND is(c, ")"));
	Test(simple_type_name(c) AND is(c, "(") AND expression_list(c) AND is(c, ")"));
	Test(simple_type_name(c) AND is(c, "(") AND is(c, ")"));
	Test(postfix_expression(c) AND is(c, ".") AND name(c));
	Test(postfix_expression(c) AND is(c, "->") AND name(c));
	Test(postfix_expression(c) AND is(c, "++"));
	Test(postfix_expression(c) AND is(c, "--"));
	return false;
}

bool unary_expression(ParseContext &c)
{
	Test(posfix_expression(c));
	Test(is(c, "++") AND unary_expression(c));
	Test(is(c, "--") AND unary_expression(c));
	Test(unary_operator(c) AND cast_expression(c));
	Test(is(c, "sizeof") AND unary_expression(c));
	Test(is(c, "sizeof") AND is(c, "(") AND type_name(c) AND is(c, ")"));
	Test(allocation_expression(c));
	Test(deallocation_expression(c));
	return false;
}

bool cast_expression(ParseContext &c)
{
	Test(unary_expression(c));
	Test(is(c, "(") AND type_name(c) AND is(c, ")") AND cast_expression(c));
	return false;
}

bool pm_expression(ParseContext &c)
{
	Test(cast_expression(c));
	Test(pm_expression(c) AND is(c, ".*") AND cast_expression(c));
	Test(pm_expression(c) AND is(c, "->*") AND cast_expression(c));
	return false;
}

bool multiplicative_expression(ParseContext &c)
{
	Test(pm_expression(c));
	Test(multiplicative_expression(c) AND is(c, "*") AND pm_expression(c));
	Test(multiplicative_expression(c) AND is(c, "/") AND pm_expression(c));
	Test(multiplicative_expression(c) AND is(c, "%") AND pm_expression(c));
	return false;
}

bool additive_expression(ParseContext &c)
{
	Test(multiplicative_expression(c));
	Test(additive_expression(c) AND is(c, "+") AND multiplicative_expression(c));
	Test(additive_expression(c) AND is(c, "-") AND multiplicative_expression(c));
	return false;
}

bool shift_expression(ParseContext &c)
{
	Test(additive_expression(c));
	Test(shift_expression(c) AND is(c, "<<") AND additive_expression(c));
	Test(shift_expression(c) AND is(c, ">>") AND additive_expression(c));
	return false;
}


bool relationa1_expression(ParseContext &c)
{
	Test(shift_expression(c));
	Test(relational_expression(c) AND is(c, "<") AND shift_expression(c));
	Test(relational_expression(c) AND is(c, ">") AND shift_expression(c));
	Test(relational_expression(c) AND is(c, "<=") AND shift_expression(c));
	Test(relational_expression(c) AND is(c, ">=") AND shift_expression(c));
	return false;
}

bool equality_expression(ParseContext &c)
{
	Test(relational_expression(c));
	Test(equality_expression(c) AND is(c, "==") AND relational_expression(c));
	Test(equality_expression(c) AND is(c, "!=") AND relational_expression(c));
	return false;
}

bool AND_expression(ParseContext &c)
{
	Test(equality_expression(c));
	Test(AND_expression(c) AND is(c, "&") AND equality_expression(c));
	return false;
}

bool exclusive_OR_expression(ParseContext &c)
{
	Test(AND_expression(c));
	Test(exclusive_OR_expression(c) AND is(c, "^") AND AND_expression(c));
	return false;
}

bool conditional_expression(ParseContext &c)
{
	Test(logical_OR_expression(c));
	Test(logical_OR_expression AND is(c, "?") AND expression(c) AND is(c, ":") AND conditional_expression(c));
	return false;
}

bool assignment_expression(ParseContext &c)
{
	Test(conditional_expression(c));
	Test(unary_expression(c) AND assignment_operator(c) AND assignment_expression(c));
	return false;
}

bool assignment_operator(ParseContext &c)
{
    return	is(c, "=")
			OR
			is(c, "*=")
			OR
			is(c, "/=")
			OR
			is(c, "%=")
			OR
			is(c, "+=")
			OR
			is(c, "-=")
			OR
			is(c, "<<=")
			OR
			is(c, ">>=")
			OR
			is(c, "&=")
			OR
			is(c, "^=")
			OR
			is(c, "|=");
}

