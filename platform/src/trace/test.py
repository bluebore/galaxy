import sqlparse

class Sql2Mdt(object):
    def convert(self, sql):
        statement = sqlparse.parse(sql)[0]
        for token in statement.tokens:
            if token.is_group():
                for t in token.get_sublists():
                    if `
                    print "%s %s is_group %s "%(t.ttype, t.value, t.is_group())
                    
if __name__ == "__main__":
    sql_convert = Sql2Mdt()
    sql_convert.convert("select name from galaxy where time=1111 and time >222")

