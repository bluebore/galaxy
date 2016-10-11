#  simple parser for parsing protobuf .proto files
#
#  Copyright 2010, Paul McGuire, modified by fxsjy 2016
#
import sys

from pyparsing import (Word, alphas, alphanums, Regex, Suppress, Forward,
    Group, oneOf, ZeroOrMore, Optional, delimitedList, Keyword,
    restOfLine, quotedString)

if len(sys.argv) < 2:
    print "usage: python pb_to_cpp.py [proto file] [class name]" 
    sys.exit(1)
class_name = ""

if len(sys.argv) > 2:
    class_name = sys.argv[2] 

ident = Word(alphas+"_",alphanums+"_").setName("identifier")
integer = Regex(r"[+-]?\d+")

LBRACE,RBRACE,LBRACK,RBRACK,LPAR,RPAR,EQ,SEMI = map(Suppress,"{}[]()=;")

kwds = """message required optional repeated enum extensions extends extend 
          to package service rpc returns true false option import"""
for kw in kwds.split():
    exec("%s_ = Keyword('%s')" % (kw.upper(), kw))

messageBody = Forward()

messageDefn = MESSAGE_ - ident("messageId") + LBRACE + messageBody("body") + RBRACE

typespec = oneOf("""double float int32 int64 uint32 uint64 sint32 sint64 
                    fixed32 fixed64 sfixed32 sfixed64 bool string bytes""") | ident
rvalue = integer | TRUE_ | FALSE_ | ident
fieldDirective = LBRACK + Group(ident + EQ + rvalue) + RBRACK
fieldDefn = (( REQUIRED_ | OPTIONAL_ | REPEATED_ )("fieldQualifier") - 
              typespec("typespec") + ident("ident") + EQ + integer("fieldint") + ZeroOrMore(fieldDirective) + SEMI)

# enumDefn ::= 'enum' ident '{' { ident '=' integer ';' }* '}'
enumDefn = ENUM_ - ident + LBRACE + ZeroOrMore( Group(ident + EQ + integer + SEMI) ) + RBRACE

# extensionsDefn ::= 'extensions' integer 'to' integer ';'
extensionsDefn = EXTENSIONS_ - integer + TO_ + integer + SEMI

# messageExtension ::= 'extend' ident '{' messageBody '}'
messageExtension = EXTEND_ - ident + LBRACE + messageBody + RBRACE

# messageBody ::= { fieldDefn | enumDefn | messageDefn | extensionsDefn | messageExtension }*
messageBody << Group(ZeroOrMore( Group(fieldDefn | enumDefn | messageDefn | extensionsDefn | messageExtension) ))

# methodDefn ::= 'rpc' ident '(' [ ident ] ')' 'returns' '(' [ ident ] ')' ';'
methodDefn = (RPC_ - ident("methodName") + 
              LPAR + Optional(ident("methodParam")) + RPAR + 
              RETURNS_ + LPAR + Optional(ident("methodReturn")) + RPAR + Optional(SEMI))

# serviceDefn ::= 'service' ident '{' methodDefn* '}'
serviceDefn = SERVICE_ - ident("serviceName") + LBRACE + ZeroOrMore(Group(methodDefn)) + RBRACE

# packageDirective ::= 'package' ident [ '.' ident]* ';'
packageDirective = Group(PACKAGE_ - delimitedList(ident, '.', combine=True) + SEMI)

comment = '//' + restOfLine

importDirective = IMPORT_ - quotedString("importFileSpec") + SEMI

optionDirective = OPTION_ - ident("optionName") + EQ + ident("optionValue") + SEMI

topLevelStatement = Group(messageDefn | messageExtension | enumDefn | serviceDefn | importDirective | optionDirective)

parser = Optional(importDirective) + Optional(packageDirective) + ZeroOrMore(topLevelStatement)

parser.ignore(comment)

proto_txt = file(sys.argv[1]).read()

from pprint import pprint
tree = parser.parseString(proto_txt, parseAll=True).asList()

type_mapping = {
    'int64': 'int64_t',
    'int32': 'int32_t',
    'uint32': 'uint32_t',
    'string': 'std::string'
}

for item in tree:
    item_type = item[0]
    item_name = item[1]
    if item_type == "message":
        struct_name = item_name
        print "struct " + struct_name + " {"
        for sub_item in item[2]:
            sub_rep = sub_item[0]
            sub_type = sub_item[1]
            sub_name = sub_item[2]
            if sub_rep != "repeated":
                print "    " + type_mapping.get(sub_type, sub_type) + " " + sub_name + ";"
            else:
                print "    " + "std::vector<" + type_mapping.get(sub_type, sub_type) + "> "  + sub_name + ";"
        print "};"

    elif item_type == "enum":
        enum_name = item_name 
        print 'enum ' +  enum_name + " {"
        for sub_item in item[2:]:
            print "    " +  sub_item[0] + '=' + sub_item[1] + ","
        print "};"
    elif item_type == "service":
        if class_name == "":
            for rpc in item[2:]:
                print "bool " + rpc[1] + "(const " + rpc[2] + "& request, " + rpc[4] + "* response);"
        else:
            for rpc in item[2:]:
                print "bool " + class_name +"::" + rpc[1] + "(const " + rpc[2] + "& request, " + rpc[4] + "* response) {\n    return false;\n}\n"
 
