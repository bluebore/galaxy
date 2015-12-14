# -*- coding:utf-8 -*-
# Copyright (c) 2015, Galaxy Authors. All Rights Reserved
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.


class SimpleSqlParser(object):
    
    def parse(self, sql):
        select_index = sql.find("select")
        if select_index < 0:
            return {}, False
        context = {"fields":[], "conditions":[]}
        from_index = sql.find("from")
        if from_index < 6:
            return {}, False
        self.handle_select(context, sql[6:from_index])
        where_index = sql.find("where")
        order_index = sql.find("order by")
        if where_index < 0:
            self.handle_table(context, sql[from_index + 4:])
        else:
            self.handle_table(context, sql[from_index + 4:where_index])
            if order_index < 0:
                self.handle_where(context, sql[where_index + 5:])
            else:
                self.handle_where(context, sql[where_index + 5: order_index])
                self.handle_order(context, sql[order_index + 8:])
        return context, True

    def handle_select(self, context, select):
        context["fields"] = select.replace(" ", "").split(",")

    def handle_table(self, context, table):
        context["table"] = table.strip()

    # just support and
    def handle_where(self, context, condition):
        conditions = condition.lower().split("and")
        for cond in conditions:
            if self.is_int(cond):
                self.handle_int_cond(cond, context)
            else:
                self.handle_str_cond(cond, context)
    def handle_order(self, context, order):
        parts = order.strip().split(" ")
        context["order"] = (parts[0].strip(), parts[1].strip())
    def is_int(self, cond):
        cond.find("'") < 0 or cond.find('"') < 0

    def handle_int_cond(self, cond, context):
        if cond.find(">=") >= 0 :
            parts = cond.split(">=")
            context["conditions"].append((parts[0].strip(), ">=", long(parts[1].strip())))
        elif cond.find(">") >= 0:
            parts = cond.split(">")
            context["conditions"].append((parts[0].strip(), ">", long(parts[1].strip())))
        elif cond.find("<=") >= 0:
            parts = cond.split("<=")
            context["conditions"].append((parts[0].strip(), "<=", long(parts[1].strip())))
        elif cond.find("<") >= 0:
            parts = cond.split("<")
            context["conditions"].append((parts[0].strip(), "<", long(parts[1].strip())))
        elif cond.find("=") >= 0:
            parts = cond.split("=")
            context["conditions"].append((parts[0].strip(), "=", long(parts[1].strip())))

    def handle_str_cond(self, cond, context):
        cond = cond.replace("'", "").replace('"',"")
        if cond.find(">=") >= 0 :
            parts = cond.split(">=")
            context["conditions"].append((parts[0].strip(), ">=", parts[1].strip()))
        elif cond.find(">") >= 0:
            parts = cond.split(">")
            context["conditions"].append((parts[0].strip(), ">", parts[1].strip()))
        elif cond.find("<=") >= 0:
            parts = cond.split("<=")
            context["conditions"].append((parts[0].strip(), "<=", parts[1].strip()))
        elif cond.find("<") >= 0:
            parts = cond.split("<")
            context["conditions"].append((parts[0].strip(), "<", parts[1].strip()))
        elif cond.find("=") >= 0:
            parts = cond.split("=")
            context["conditions"].append((parts[0].strip(), "=", parts[1].strip()))

if __name__ == "__main__":
    parser = SimpleSqlParser()
    print parser.parse("select name,id from JobStat where id='dasasdas' and time >= '2015-11-10 20:56' and time <='2015-11-10 22:22' order by time desc")
